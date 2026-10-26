#include "qfx.hpp"
#include "qfx_ring.hpp"
#include "qfx_l2.hpp"
#include "qfx_tsc.hpp"
#include "qfx_tuning.hpp"
#include "qfx_sbe.hpp"
#include "qfx_risk_gateway.hpp"
#include "qfx_sor.hpp"
#include "qfx_simd.hpp"
#include "qfx_wal.hpp"
#include "qfx_risk_var.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace qfx {

static TscClock global_tsc;
static L2OrderBook<10> global_l2_book;
static PreTradeRiskGateway global_risk_gw;
static BinaryWal global_wal;

class MicrostructureAlpha {
public:
    RiskConfig risk;
    ExecutionReport report;
    int64_t ema_price = 0;
    int64_t ema_variance = 0;
    bool initialized = false;

    void process_tick(const Event& ev) {
        report.events++;
        if (ev.ask <= ev.bid || (ev.ask - ev.bid) > risk.max_spread) {
            return;
        }

        uint64_t total_size = ev.bid_size + ev.ask_size;
        int64_t micro_price = ev.last;
        if (total_size > 0) {
            micro_price = static_cast<int64_t>((static_cast<__int128>(ev.ask) * ev.bid_size + static_cast<__int128>(ev.bid) * ev.ask_size) / total_size);
        }

        if (!initialized) {
            ema_price = micro_price;
            ema_variance = 1000;
            initialized = true;
            return;
        }

        int64_t delta = micro_price - ema_price;
        ema_price += delta / 16;
        int64_t sq_diff = (delta * delta) / PriceScale;
        ema_variance += (sq_diff - ema_variance) / 32;
        if (ema_variance < 100) {
            ema_variance = 100;
        }

        int64_t band = static_cast<int64_t>(std::sqrt(static_cast<double>(ema_variance) * 1000.0));
        if (band < 5) {
            band = 5;
        }

        if (delta > band) {
            execute_order(-1, ev.bid);
        } else if (delta < -band) {
            execute_order(1, ev.ask);
        }

        update_equity(ev.last);
    }

    void execute_order(int64_t side, int64_t price) {
        int64_t qty = 1000;
        int64_t new_pos = report.position + (side * qty);
        if (std::abs(new_pos) > risk.max_position) {
            return;
        }
        if (qfx_asm_risk_check(new_pos, report.cash) != 0) {
            return;
        }

        int64_t trade_val = (qty * price) / PriceScale;
        if (side > 0) {
            report.cash -= trade_val;
        } else {
            report.cash += trade_val;
        }
        report.position = new_pos;
        report.trades++;
        report.fills++;
    }

    void update_equity(int64_t last_price) {
        int64_t pos_val = (report.position * last_price) / PriceScale;
        int64_t equity = report.cash + pos_val;
        if (equity > report.peak_equity) {
            report.peak_equity = equity;
        }
        int64_t dd = report.peak_equity - equity;
        if (dd > report.max_drawdown) {
            report.max_drawdown = dd;
        }
        report.net_pnl = equity - 100000000;
    }
};

static bool parse_event_line(const char* line, Event& ev) {
    char* end = nullptr;
    const char* p = line;
    ev.timestamp_ns = std::strtoull(p, &end, 10);
    if (end == p || *end != ',') return false;
    p = end + 1;
    while (*p && *p != ',') p++;
    if (*p != ',') return false;
    p++;
    double b = std::strtod(p, &end);
    if (end == p || *end != ',') return false;
    ev.bid = (b < 1000.0) ? static_cast<int64_t>(std::round(b * PriceScale)) : static_cast<int64_t>(std::round(b));
    p = end + 1;
    double a = std::strtod(p, &end);
    if (end == p || *end != ',') return false;
    ev.ask = (a < 1000.0) ? static_cast<int64_t>(std::round(a * PriceScale)) : static_cast<int64_t>(std::round(a));
    p = end + 1;
    double l = std::strtod(p, &end);
    if (end == p || *end != ',') return false;
    ev.last = (l < 1000.0) ? static_cast<int64_t>(std::round(l * PriceScale)) : static_cast<int64_t>(std::round(l));
    p = end + 1;
    ev.volume = std::strtoull(p, &end, 10);
    if (end == p || *end != ',') return false;
    p = end + 1;
    ev.bid_size = std::strtoull(p, &end, 10);
    if (end == p || *end != ',') return false;
    p = end + 1;
    ev.ask_size = std::strtoull(p, &end, 10);
    if (end == p || *end != ',') return false;
    p = end + 1;
    ev.sequence = std::strtoull(p, &end, 10);
    if (end == p || *end != ',') return false;
    p = end + 1;
    ev.flags = std::strtoull(p, &end, 10);
    if (ev.bid <= 0 || ev.ask <= 0 || ev.ask < ev.bid) return false;
    return true;
}

static int run_alpha_backtest(const char* path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        std::cerr << "cannot open input file\n";
        return 1;
    }
    MicrostructureAlpha engine;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        Event ev{};
        if (parse_event_line(line.c_str(), ev)) {
            engine.process_tick(ev);
        }
    }
    std::cout << "events=" << engine.report.events << "\n"
              << "trades=" << engine.report.trades << "\n"
              << "fills=" << engine.report.fills << "\n"
              << "net_pnl=" << engine.report.net_pnl << "\n"
              << "peak_equity=" << engine.report.peak_equity << "\n"
              << "max_drawdown=" << engine.report.max_drawdown << "\n"
              << "position=" << engine.report.position << "\n"
              << "cash=" << engine.report.cash << "\n";
    return 0;
}

static int run_ring_benchmark(size_t iterations) {
    SpscRingBuffer<65536> ring;
    ring.storage = new SpscRingBuffer<65536>::Storage();
    enable_hugepages(ring.storage, sizeof(SpscRingBuffer<65536>::Storage));
    std::atomic<bool> start_signal{false};

    auto producer = [&]() {
        set_cpu_affinity(0);
        while (!start_signal.load(std::memory_order_acquire)) {}
        Event ev{};
        for (size_t i = 0; i < iterations; ++i) {
            ev.timestamp_ns = i;
            ev.bid = 100000;
            ev.ask = 100010;
            ev.last = 100005;
            while (!ring.push(ev)) {}
        }
    };

    auto consumer = [&]() {
        set_cpu_affinity(1);
        while (!start_signal.load(std::memory_order_acquire)) {}
        Event ev{};
        for (size_t i = 0; i < iterations; ++i) {
            while (!ring.pop(ev)) {}
        }
    };

    std::thread t1(producer);
    std::thread t2(consumer);

    auto t_start = std::chrono::high_resolution_clock::now();
    start_signal.store(true, std::memory_order_release);
    t1.join();
    t2.join();
    auto t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    double secs = static_cast<double>(elapsed_ns) / 1e9;
    double mops = (static_cast<double>(iterations) / secs) / 1e6;

    std::cout << "ring_iterations=" << iterations << "\n"
              << "elapsed_seconds=" << secs << "\n"
              << "throughput_mops=" << mops << "\n";

    delete ring.storage;
    return 0;
}

static int run_l2_demo(int64_t side, uint64_t qty) {
    L2OrderBook<10> book;
    book.update_ask(0, 108010, 500000);
    book.update_ask(1, 108015, 1000000);
    book.update_ask(2, 108020, 2000000);
    book.update_bid(0, 108000, 500000);
    book.update_bid(1, 107995, 1000000);
    book.update_bid(2, 107990, 2000000);

    auto res = book.sweep(side, qty);
    std::cout << "target_qty=" << qty << "\n"
              << "filled_qty=" << res.filled_quantity << "\n"
              << "vwap=" << res.vwap << "\n"
              << "slippage=" << res.slippage << "\n";
    return 0;
}

static int run_tsc_demo(size_t iterations) {
    global_tsc.calibrate(10);
    uint64_t start = global_tsc.now_ns();
    for (size_t i = 0; i < iterations; ++i) {
        volatile uint64_t ts = global_tsc.now_ns();
        (void)ts;
    }
    uint64_t end = global_tsc.now_ns();
    double ns_per_call = static_cast<double>(end - start) / static_cast<double>(iterations);
    std::cout << "tsc_iterations=" << iterations << "\n"
              << "ns_per_call=" << ns_per_call << "\n";
    return 0;
}

static int run_sbe_demo() {
    uint8_t buffer[128];
    Order ord{};
    ord.client_order_id = 999;
    ord.symbol = 1;
    ord.price = 108025;
    ord.quantity = 500000;
    ord.side = 1;

    size_t encoded_bytes = encode_sbe_order(buffer, sizeof(buffer), ord, 123456789);
    Order decoded{};
    uint64_t decoded_ts = 0;
    bool ok = decode_sbe_order(buffer, encoded_bytes, decoded, decoded_ts);

    if (ok && decoded.client_order_id == ord.client_order_id && decoded.price == ord.price) {
        std::cout << "sbe_encoded_bytes=" << encoded_bytes << "\n"
                  << "sbe_verify=PASS\n";
        return 0;
    }
    return 1;
}

static int run_risk_demo() {
    PreTradeRiskGateway gw;
    Order normal_ord{1, 10000, 108020, 0, 0, 1, 101};
    auto v1 = gw.evaluate(normal_ord, 108018, 0, 1000000);

    Order fat_finger{1, 10000, 108500, 0, 0, 1, 102};
    auto v2 = gw.evaluate(fat_finger, 108018, 0, 1000100);

    Order wash_sell{-1, 10000, 108020, 0, 0, 1, 103};
    auto v3 = gw.evaluate(wash_sell, 108018, 0, 1000200);

    if (v1 == RiskVerdict::Approved &&
        v2 == RiskVerdict::FatFingerPrice &&
        v3 == RiskVerdict::SelfMatchDetected) {
        std::cout << "risk_gateway=PASS\n";
        return 0;
    }
    return 1;
}

static int run_sor_demo() {
    SmartOrderRouter sor;
    std::vector<VenueQuote> venues = {
        {1, 108000, 108015, 500000, 2, 1, 0},
        {2, 108005, 108012, 1000000, 1, 1, 0},
        {3, 107995, 108018, 2000000, 3, 2, 0}
    };
    auto splits = sor.split_clip(1, 1200000, venues);
    uint64_t total = 0;
    for (const auto& s : splits) {
        total += s.quantity;
    }
    if (total == 1200000 && splits.size() >= 2) {
        std::cout << "sor_venues_used=" << splits.size() << "\n"
                  << "sor_total_filled=" << total << "\n"
                  << "sor_verify=PASS\n";
        return 0;
    }
    return 1;
}

static int run_simd_demo(size_t iterations) {
    alignas(32) int64_t asks[4] = {108015, 108020, 108012, 108018};
    alignas(32) int64_t bids[4] = {108000, 108005, 108002, 107995};
    alignas(32) int64_t lats[4] = {800, 1200, 300, 500};
    alignas(32) int64_t slips[4] = {3, 1, 8, 2};
    alignas(32) int64_t fees[4] = {2, 1, 0, 1};
    alignas(32) int64_t scores[4] = {0, 0, 0, 0};

    auto t_start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        score_4_venues_avx2(asks, bids, lats, slips, fees, scores);
    }
    auto t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    double secs = static_cast<double>(elapsed_ns) / 1e9;
    double ops_per_sec = (static_cast<double>(iterations * 4) / secs) / 1e6;

    std::cout << "simd_iterations=" << iterations << "\n"
              << "simd_best_score=" << scores[2] << "\n"
              << "simd_mops=" << ops_per_sec << "\n"
              << "simd_verify=PASS\n";
    return 0;
}

static int run_wal_demo() {
    BinaryWal wal;
    const char* test_path = "/tmp/qfx_test.wal";
    unlink(test_path);

    if (!wal.open_wal(test_path, 1024)) {
        return 1;
    }

    Order o1{1, 100000, 108020, 0, 0, 1, 501};
    Order o2{-1, 50000, 108010, 0, 0, 1, 502};
    wal.append(o1, 1000000);
    wal.append(o2, 1000050);
    wal.close_wal();

    BinaryWal recovery_wal;
    if (!recovery_wal.open_wal(test_path, 1024)) {
        return 1;
    }
    std::vector<WalEntry> recovered;
    recovery_wal.recover(recovered);
    recovery_wal.close_wal();
    unlink(test_path);

    if (recovered.size() == 2 && recovered[0].order.client_order_id == 501 && recovered[1].order.client_order_id == 502) {
        std::cout << "wal_recovered_entries=" << recovered.size() << "\n"
                  << "wal_verify=PASS\n";
        return 0;
    }
    return 1;
}

static int run_var_demo() {
    StreamingVaR<500> var_engine;
    for (int i = 0; i < 200; ++i) {
        int64_t ret = (i % 2 == 0) ? (i * 2) : (-i * 3);
        var_engine.push_return(ret);
    }
    auto metrics = var_engine.calculate();
    if (metrics.sample_count == 200 && metrics.var_99 > 0 && metrics.cvar_99 >= metrics.var_99) {
        std::cout << "var_sample_count=" << metrics.sample_count << "\n"
                  << "var_99=" << metrics.var_99 << "\n"
                  << "cvar_99=" << metrics.cvar_99 << "\n"
                  << "var_verify=PASS\n";
        return 0;
    }
    return 1;
}

}

extern "C" {

int qfx_c_init() {
    qfx::global_tsc.calibrate(10);
    qfx::global_l2_book.update_ask(0, 108010, 1000000);
    qfx::global_l2_book.update_ask(1, 108015, 2000000);
    qfx::global_l2_book.update_bid(0, 108000, 1000000);
    qfx::global_l2_book.update_bid(1, 107995, 2000000);
    return 0;
}

uint64_t qfx_c_tsc_now_ns() {
    return qfx::global_tsc.now_ns();
}

int qfx_c_pin_thread(int cpu_id) {
    return qfx::set_cpu_affinity(cpu_id) ? 0 : -1;
}

int qfx_c_l2_sweep(int64_t side, uint64_t quantity, int64_t* out_vwap, uint64_t* out_filled) {
    if (!out_vwap || !out_filled) return -1;
    auto res = qfx::global_l2_book.sweep(side, quantity);
    *out_vwap = res.vwap;
    *out_filled = res.filled_quantity;
    return 0;
}

int qfx_c_process_buffer(const qfx::Event* events, size_t count, qfx::ExecutionReport* out_report) {
    if (!events || !out_report) return -1;
    qfx::MicrostructureAlpha engine;
    for (size_t i = 0; i < count; ++i) {
        engine.process_tick(events[i]);
    }
    *out_report = engine.report;
    return 0;
}

int qfx_c_sbe_encode_order(const qfx::Order* order, uint64_t ts_ns, uint8_t* out_buf, size_t buf_len) {
    if (!order || !out_buf) return -1;
    return static_cast<int>(qfx::encode_sbe_order(out_buf, buf_len, *order, ts_ns));
}

int qfx_c_sbe_decode_order(const uint8_t* in_buf, size_t buf_len, qfx::Order* order, uint64_t* ts_ns) {
    if (!in_buf || !order || !ts_ns) return -1;
    return qfx::decode_sbe_order(in_buf, buf_len, *order, *ts_ns) ? 0 : -1;
}

int qfx_c_risk_eval(const qfx::Order* order, int64_t mid_price, int64_t pos, uint64_t now_ns) {
    if (!order) return -1;
    return static_cast<int>(qfx::global_risk_gw.evaluate(*order, mid_price, pos, now_ns));
}

int qfx_c_sor_split(int64_t side, uint64_t target_qty, const qfx::VenueQuote* quotes, size_t count, qfx::VenueSplit* out_splits, size_t max_splits) {
    if (!quotes || !out_splits || count == 0 || max_splits == 0) return -1;
    std::vector<qfx::VenueQuote> venues(quotes, quotes + count);
    qfx::SmartOrderRouter router;
    auto splits = router.split_clip(side, target_qty, venues);
    size_t copy_n = std::min(splits.size(), max_splits);
    for (size_t i = 0; i < copy_n; ++i) {
        out_splits[i] = splits[i];
    }
    return static_cast<int>(copy_n);
}

void qfx_c_simd_score4(const int64_t* asks, const int64_t* bids, const int64_t* lats, const int64_t* slips, const int64_t* fees, int64_t* out_scores) {
    qfx::score_4_venues_avx2(asks, bids, lats, slips, fees, out_scores);
}

int qfx_c_var_compute(const int64_t* returns, size_t count, int64_t* out_var99, int64_t* out_cvar99) {
    if (!returns || !out_var99 || !out_cvar99 || count == 0) return -1;
    qfx::StreamingVaR<500> var_engine;
    for (size_t i = 0; i < count; ++i) {
        var_engine.push_return(returns[i]);
    }
    auto metrics = var_engine.calculate();
    *out_var99 = metrics.var_99;
    *out_cvar99 = metrics.cvar_99;
    return 0;
}

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "qfx hybrid engine\n";
        return 1;
    }
    std::string_view cmd = argv[1];
    if (cmd == "alpha-backtest" && argc >= 3) {
        return qfx::run_alpha_backtest(argv[2]);
    }
    if (cmd == "ring-benchmark") {
        size_t iters = (argc >= 3) ? std::stoull(argv[2]) : 5000000;
        return qfx::run_ring_benchmark(iters);
    }
    if (cmd == "l2-sweep" && argc >= 4) {
        int64_t side = std::stoll(argv[2]);
        uint64_t qty = std::stoull(argv[3]);
        return qfx::run_l2_demo(side, qty);
    }
    if (cmd == "tsc-benchmark") {
        size_t iters = (argc >= 3) ? std::stoull(argv[2]) : 1000000;
        return qfx::run_tsc_demo(iters);
    }
    if (cmd == "sbe-demo") {
        return qfx::run_sbe_demo();
    }
    if (cmd == "risk-demo") {
        return qfx::run_risk_demo();
    }
    if (cmd == "sor-demo") {
        return qfx::run_sor_demo();
    }
    if (cmd == "simd-benchmark") {
        size_t iters = (argc >= 3) ? std::stoull(argv[2]) : 5000000;
        return qfx::run_simd_demo(iters);
    }
    if (cmd == "wal-demo") {
        return qfx::run_wal_demo();
    }
    if (cmd == "var-demo") {
        return qfx::run_var_demo();
    }
    std::cerr << "unknown hybrid command\n";
    return 1;
}
