#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

namespace qfx {

struct alignas(8) VenueQuote {
    uint32_t venue_id = 0;
    int64_t bid = 0;
    int64_t ask = 0;
    uint64_t capacity = 0;
    int64_t fee = 0;
    int64_t slippage = 0;
    int64_t score = 0;
};

struct VenueSplit {
    uint32_t venue_id = 0;
    uint64_t quantity = 0;
    int64_t expected_price = 0;
};

class SmartOrderRouter {
public:
    std::vector<VenueSplit> split_clip(int64_t side, uint64_t target_qty, std::vector<VenueQuote>& venues) {
        std::vector<VenueSplit> splits;
        if (target_qty == 0 || venues.empty()) return splits;

        for (auto& v : venues) {
            int64_t base_px = (side > 0) ? v.ask : v.bid;
            v.score = (side > 0) ? (base_px + v.fee + v.slippage) : (-base_px + v.fee + v.slippage);
        }

        std::sort(venues.begin(), venues.end(), [](const VenueQuote& a, const VenueQuote& b) {
            return a.score < b.score;
        });

        uint64_t remaining = target_qty;
        for (const auto& v : venues) {
            if (remaining == 0) break;
            if (v.capacity == 0) continue;

            uint64_t allocate = std::min(remaining, v.capacity);
            splits.push_back(VenueSplit{
                v.venue_id,
                allocate,
                (side > 0) ? v.ask : v.bid
            });
            remaining -= allocate;
        }

        return splits;
    }

    std::vector<Order> slice_twap(const Order& parent, uint64_t duration_ns, size_t num_slices, uint64_t start_ns) {
        std::vector<Order> slices;
        if (num_slices == 0 || parent.quantity == 0) return slices;

        uint64_t slice_qty = parent.quantity / num_slices;
        uint64_t remainder = parent.quantity % num_slices;
        uint64_t step_ns = duration_ns / num_slices;

        for (size_t i = 0; i < num_slices; ++i) {
            Order child = parent;
            child.quantity = slice_qty + (i == num_slices - 1 ? remainder : 0);
            child.client_order_id = parent.client_order_id * 1000 + i;
            slices.push_back(child);
            (void)start_ns;
            (void)step_ns;
        }

        return slices;
    }
};

}
