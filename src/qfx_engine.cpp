#include "qfx.hpp"
#include "qfx_ring.hpp"
#include "qfx_l2.hpp"
#include "qfx_tsc.hpp"
#include "qfx_tuning.hpp"
#include "qfx_sbe.hpp"
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
    std::cerr << "unknown hybrid command\n";
    return 1;
}
