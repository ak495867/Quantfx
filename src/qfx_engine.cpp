#include "qfx.hpp"
#include "qfx_ring.hpp"
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
    std::atomic<bool> start_signal{false};

    auto producer = [&]() {
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
    std::cerr << "unknown hybrid command\n";
    return 1;
}
