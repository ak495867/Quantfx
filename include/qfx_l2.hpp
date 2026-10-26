#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

namespace qfx {

struct alignas(8) L2Level {
    int64_t price = 0;
    uint64_t size = 0;
};

struct SweepResult {
    int64_t vwap = 0;
    uint64_t filled_quantity = 0;
    uint64_t unfilled_quantity = 0;
    int64_t slippage = 0;
};

template <size_t MaxDepth = 10>
class alignas(64) L2OrderBook {
public:
    std::array<L2Level, MaxDepth> bids{};
    std::array<L2Level, MaxDepth> asks{};
    size_t bid_depth = 0;
    size_t ask_depth = 0;

    void clear() noexcept {
        bid_depth = 0;
        ask_depth = 0;
        bids.fill(L2Level{});
        asks.fill(L2Level{});
    }

    void update_bid(size_t level, int64_t price, uint64_t size) noexcept {
        if (level >= MaxDepth) return;
        bids[level] = L2Level{price, size};
        if (level >= bid_depth && size > 0) {
            bid_depth = level + 1;
        }
    }

    void update_ask(size_t level, int64_t price, uint64_t size) noexcept {
        if (level >= MaxDepth) return;
        asks[level] = L2Level{price, size};
        if (level >= ask_depth && size > 0) {
            ask_depth = level + 1;
        }
    }

    SweepResult sweep(int64_t side, uint64_t target_quantity) const noexcept {
        SweepResult result{};
        if (target_quantity == 0) return result;

        uint64_t remaining = target_quantity;
        __int128 total_cost = 0;
        uint64_t total_filled = 0;

        if (side > 0) {
            int64_t top_price = (ask_depth > 0) ? asks[0].price : 0;
            for (size_t i = 0; i < ask_depth && remaining > 0; ++i) {
                if (asks[i].size == 0) continue;
                uint64_t take = std::min(remaining, asks[i].size);
                total_cost += static_cast<__int128>(take) * asks[i].price;
                total_filled += take;
                remaining -= take;
            }
            result.filled_quantity = total_filled;
            result.unfilled_quantity = remaining;
            if (total_filled > 0) {
                result.vwap = static_cast<int64_t>(total_cost / total_filled);
                result.slippage = result.vwap - top_price;
            }
        } else {
            int64_t top_price = (bid_depth > 0) ? bids[0].price : 0;
            for (size_t i = 0; i < bid_depth && remaining > 0; ++i) {
                if (bids[i].size == 0) continue;
                uint64_t take = std::min(remaining, bids[i].size);
                total_cost += static_cast<__int128>(take) * bids[i].price;
                total_filled += take;
                remaining -= take;
            }
            result.filled_quantity = total_filled;
            result.unfilled_quantity = remaining;
            if (total_filled > 0) {
                result.vwap = static_cast<int64_t>(total_cost / total_filled);
                result.slippage = top_price - result.vwap;
            }
        }

        return result;
    }
};

}
