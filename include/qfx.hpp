#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <array>

namespace qfx {

constexpr uint32_t Magic = 0x31584651;
constexpr uint64_t Version = 1;
constexpr uint64_t HeaderSize = 64;
constexpr uint64_t BlockSize = 65536;
constexpr int64_t PriceScale = 100000;
constexpr size_t EventSize = 80;
constexpr size_t OrderSize = 56;

struct alignas(8) Event {
    uint64_t timestamp_ns;
    uint64_t symbol;
    int64_t bid;
    int64_t ask;
    int64_t last;
    uint64_t volume;
    uint64_t bid_size;
    uint64_t ask_size;
    uint64_t sequence;
    uint64_t flags;
};

static_assert(sizeof(Event) == 80, "Event struct must match EVENT_SIZE (80 bytes)");

struct alignas(8) Order {
    int64_t side;
    uint64_t quantity;
    int64_t price;
    int64_t stop_loss;
    int64_t take_profit;
    uint64_t symbol;
    uint64_t client_order_id;
};

static_assert(sizeof(Order) == 56, "Order struct must match ORDER_SIZE (56 bytes)");

struct alignas(8) Header {
    uint64_t magic;
    uint64_t version;
    uint64_t event_size;
    uint64_t scale;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t record_count;
    uint64_t reserved;
};

static_assert(sizeof(Header) == 64, "Header struct must match QFX_HEADER_SIZE (64 bytes)");

struct RiskConfig {
    int64_t max_position = 1000000;
    int64_t max_order = 100000;
    int64_t max_spread = 500;
    int64_t max_drawdown = 20000000;
    int64_t min_cash = 1000;
    int64_t threshold = 10;
};

struct ExecutionReport {
    uint64_t events = 0;
    uint64_t trades = 0;
    uint64_t fills = 0;
    uint64_t wins = 0;
    uint64_t losses = 0;
    int64_t net_pnl = 0;
    int64_t peak_equity = 100000000;
    int64_t max_drawdown = 0;
    int64_t position = 0;
    int64_t cash = 100000000;
};

extern "C" {
int32_t qfx_asm_risk_check(int64_t position, int64_t cash);
int64_t qfx_asm_fast_score(int64_t bid, int64_t ask, int64_t latency, int64_t slippage, int64_t fee);
}

}
