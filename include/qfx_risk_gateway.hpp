#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace qfx {

class TokenBucketRateLimiter {
public:
    uint64_t max_tokens = 500;
    uint64_t tokens = 500;
    uint64_t fill_interval_ns = 10000000;
    uint64_t last_update_ns = 0;

    void configure(uint64_t max_t, uint64_t interval_ns) noexcept {
        max_tokens = max_t;
        tokens = max_t;
        fill_interval_ns = interval_ns;
        last_update_ns = 0;
    }

    bool allow(uint64_t now_ns, uint64_t cost = 1) noexcept {
        if (last_update_ns == 0) {
            last_update_ns = now_ns;
        }
        if (now_ns > last_update_ns) {
            uint64_t elapsed = now_ns - last_update_ns;
            uint64_t add_tokens = (elapsed / fill_interval_ns) * max_tokens;
            if (add_tokens > 0) {
                tokens = std::min(max_tokens, tokens + add_tokens);
                last_update_ns = now_ns;
            }
        }
        if (tokens >= cost) {
            tokens -= cost;
            return true;
        }
        return false;
    }
};

enum class RiskVerdict : int32_t {
    Approved = 0,
    RateLimitExceeded = 1,
    FatFingerPrice = 2,
    ExceedsNotional = 3,
    SelfMatchDetected = 4,
    PositionLimitExceeded = 5
};

class PreTradeRiskGateway {
public:
    int64_t max_order_notional = 5000000000;
    int64_t max_price_deviation_pips = 100;
    int64_t max_position = 1000000;
    TokenBucketRateLimiter rate_limiter;
    std::vector<Order> resting_orders;

    RiskVerdict evaluate(const Order& order, int64_t mid_price, int64_t current_pos, uint64_t now_ns) {
        if (!rate_limiter.allow(now_ns, 1)) {
            return RiskVerdict::RateLimitExceeded;
        }

        int64_t notional = static_cast<int64_t>((static_cast<__int128>(order.quantity) * order.price) / PriceScale);
        if (notional > max_order_notional) {
            return RiskVerdict::ExceedsNotional;
        }

        if (mid_price > 0) {
            int64_t diff = std::abs(order.price - mid_price);
            if (diff > max_price_deviation_pips) {
                return RiskVerdict::FatFingerPrice;
            }
        }

        int64_t new_pos = current_pos + (order.side * static_cast<int64_t>(order.quantity));
        if (std::abs(new_pos) > max_position) {
            return RiskVerdict::PositionLimitExceeded;
        }

        for (auto it = resting_orders.begin(); it != resting_orders.end();) {
            if (it->symbol == order.symbol && it->side == -order.side) {
                if ((order.side > 0 && order.price >= it->price) ||
                    (order.side < 0 && order.price <= it->price)) {
                    it = resting_orders.erase(it);
                    return RiskVerdict::SelfMatchDetected;
                }
            }
            ++it;
        }

        resting_orders.push_back(order);
        return RiskVerdict::Approved;
    }

    void clear() noexcept {
        resting_orders.clear();
    }
};

}
