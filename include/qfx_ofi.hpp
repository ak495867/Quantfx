#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <algorithm>
#include <array>

namespace qfx {

struct alignas(8) OfiSignal {
    int64_t l1_ofi = 0;
    int64_t multi_level_ofi = 0;
    int64_t predicted_alpha = 0;
};

template <size_t Depth = 5>
class OrderFlowImbalanceTracker {
private:
    struct LevelState {
        int64_t bid_price = 0;
        uint64_t bid_qty = 0;
        int64_t ask_price = 0;
        uint64_t ask_qty = 0;
    };

    std::array<LevelState, Depth> prev_depth_{};
    bool initialized_ = false;

    int64_t compute_level_ofi(
        int64_t prev_bid_p, uint64_t prev_bid_q, int64_t curr_bid_p, uint64_t curr_bid_q,
        int64_t prev_ask_p, uint64_t prev_ask_q, int64_t curr_ask_p, uint64_t curr_ask_q
    ) noexcept {
        int64_t bid_flow = 0;
        if (curr_bid_p > prev_bid_p) {
            bid_flow = static_cast<int64_t>(curr_bid_q);
        } else if (curr_bid_p == prev_bid_p) {
            bid_flow = static_cast<int64_t>(curr_bid_q) - static_cast<int64_t>(prev_bid_q);
        }

        int64_t ask_flow = 0;
        if (curr_ask_p < prev_ask_p) {
            ask_flow = static_cast<int64_t>(curr_ask_q);
        } else if (curr_ask_p == prev_ask_p) {
            ask_flow = static_cast<int64_t>(curr_ask_q) - static_cast<int64_t>(prev_ask_q);
        }

        return bid_flow - ask_flow;
    }

public:
    void reset() noexcept {
        prev_depth_.fill(LevelState{});
        initialized_ = false;
    }

    OfiSignal update(
        const int64_t* bid_prices, const uint64_t* bid_sizes,
        const int64_t* ask_prices, const uint64_t* ask_sizes,
        size_t count
    ) noexcept {
        size_t active_levels = std::min(count, Depth);
        if (!initialized_) {
            for (size_t i = 0; i < active_levels; ++i) {
                prev_depth_[i].bid_price = bid_prices[i];
                prev_depth_[i].bid_qty = bid_sizes[i];
                prev_depth_[i].ask_price = ask_prices[i];
                prev_depth_[i].ask_qty = ask_sizes[i];
            }
            initialized_ = true;
            return OfiSignal{};
        }

        OfiSignal signal{};
        int64_t weighted_ofi = 0;

        for (size_t i = 0; i < active_levels; ++i) {
            int64_t lvl_ofi = compute_level_ofi(
                prev_depth_[i].bid_price, prev_depth_[i].bid_qty,
                bid_prices[i], bid_sizes[i],
                prev_depth_[i].ask_price, prev_depth_[i].ask_qty,
                ask_prices[i], ask_sizes[i]
            );

            if (i == 0) {
                signal.l1_ofi = lvl_ofi;
            }

            int64_t weight_shift = static_cast<int64_t>(i);
            weighted_ofi += (lvl_ofi >> weight_shift);

            prev_depth_[i].bid_price = bid_prices[i];
            prev_depth_[i].bid_qty = bid_sizes[i];
            prev_depth_[i].ask_price = ask_prices[i];
            prev_depth_[i].ask_qty = ask_sizes[i];
        }

        signal.multi_level_ofi = weighted_ofi;
        signal.predicted_alpha = (weighted_ofi * 5) / 10000;
        return signal;
    }
};

struct alignas(8) RestingOrderQueue {
    uint64_t client_order_id = 0;
    int64_t price = 0;
    uint64_t order_qty = 0;
    uint64_t remaining_qty = 0;
    uint64_t queue_ahead = 0;
    int8_t side = 0;
    bool active = false;

    void place(uint64_t cl_id, int64_t px, uint64_t qty, int8_t s, uint64_t ahead) noexcept {
        client_order_id = cl_id;
        price = px;
        order_qty = qty;
        remaining_qty = qty;
        queue_ahead = ahead;
        side = s;
        active = true;
    }

    uint64_t on_trade_at_level(uint64_t traded_qty) noexcept {
        if (!active || remaining_qty == 0) return 0;
        if (queue_ahead >= traded_qty) {
            queue_ahead -= traded_qty;
            return 0;
        }

        uint64_t fillable = traded_qty - queue_ahead;
        queue_ahead = 0;
        uint64_t fill = std::min(fillable, remaining_qty);
        remaining_qty -= fill;
        if (remaining_qty == 0) {
            active = false;
        }
        return fill;
    }
};

}
