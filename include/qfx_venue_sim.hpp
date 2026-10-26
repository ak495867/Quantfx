#pragma once

#include "qfx.hpp"
#include "qfx_l2.hpp"
#include "qfx_sbe.hpp"
#include "qfx_ofi.hpp"
#include <cstdint>
#include <algorithm>
#include <array>

namespace qfx {

struct alignas(8) SbeExecutionReport {
    SbeHeader header{sizeof(SbeExecutionReport), 102, 1, 1};
    uint64_t timestamp_ns = 0;
    uint64_t client_order_id = 0;
    uint64_t exec_id = 0;
    int64_t exec_price = 0;
    uint64_t exec_quantity = 0;
    uint8_t exec_type = 0;
    int8_t side = 0;
    uint16_t reserved = 0;
    uint32_t padding = 0;
};

static_assert(sizeof(SbeExecutionReport) == 56, "SbeExecutionReport must be 56 bytes");

struct alignas(8) PassiveRestingOrder {
    uint64_t client_order_id = 0;
    int64_t price = 0;
    uint64_t initial_qty = 0;
    uint64_t filled_qty = 0;
    uint64_t queue_ahead = 0;
    int8_t side = 0;
    bool active = false;
    int64_t fill_price = 0;
    uint64_t fill_timestamp_ns = 0;
    int64_t markout_pnl = 0;
};

class VenueSimulator {
public:
    L2OrderBook<10> book;
    uint64_t exec_id_counter = 1;
    static constexpr size_t MaxRestingOrders = 16;
    std::array<PassiveRestingOrder, MaxRestingOrders> resting_orders{};

    void seed_default_market(int64_t mid = 108000) noexcept {
        book.clear();
        book.update_bid(0, mid - 5, 500000);
        book.update_bid(1, mid - 10, 1000000);
        book.update_bid(2, mid - 15, 2000000);
        book.update_ask(0, mid + 5, 500000);
        book.update_ask(1, mid + 10, 1000000);
        book.update_ask(2, mid + 15, 2000000);
        for (auto& ord : resting_orders) {
            ord = PassiveRestingOrder{};
        }
    }

    int place_passive_order(uint64_t client_order_id, int8_t side, int64_t price, uint64_t quantity) noexcept {
        for (size_t i = 0; i < MaxRestingOrders; ++i) {
            if (!resting_orders[i].active) {
                uint64_t ahead = 0;
                for (size_t lvl = 0; lvl < 10; ++lvl) {
                    if (side == 1) {
                        if (book.bids[lvl].price == price) {
                            ahead = book.bids[lvl].size;
                            break;
                        }
                    } else {
                        if (book.asks[lvl].price == price) {
                            ahead = book.asks[lvl].size;
                            break;
                        }
                    }
                }
                resting_orders[i].client_order_id = client_order_id;
                resting_orders[i].price = price;
                resting_orders[i].initial_qty = quantity;
                resting_orders[i].filled_qty = 0;
                resting_orders[i].queue_ahead = ahead;
                resting_orders[i].side = side;
                resting_orders[i].active = true;
                resting_orders[i].fill_price = 0;
                resting_orders[i].fill_timestamp_ns = 0;
                resting_orders[i].markout_pnl = 0;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    SbeExecutionReport on_market_trade(int8_t trade_side, int64_t trade_price, uint64_t trade_qty, uint64_t now_ns) noexcept {
        SbeExecutionReport report{};
        report.timestamp_ns = now_ns;

        for (auto& ord : resting_orders) {
            if (!ord.active) continue;

            if (ord.side != trade_side) {
                bool eligible = (ord.side == 1 && trade_price <= ord.price) ||
                                (ord.side == -1 && trade_price >= ord.price);
                if (eligible) {
                    if (ord.queue_ahead >= trade_qty) {
                        ord.queue_ahead -= trade_qty;
                    } else {
                        uint64_t fillable = trade_qty - ord.queue_ahead;
                        ord.queue_ahead = 0;
                        uint64_t remaining = ord.initial_qty - ord.filled_qty;
                        uint64_t fill = std::min(fillable, remaining);
                        ord.filled_qty += fill;
                        ord.fill_price = ord.price;
                        ord.fill_timestamp_ns = now_ns;

                        report.client_order_id = ord.client_order_id;
                        report.exec_id = exec_id_counter++;
                        report.side = ord.side;
                        report.exec_price = ord.price;
                        report.exec_quantity = fill;
                        report.exec_type = (ord.filled_qty >= ord.initial_qty) ? 2 : 1;

                        if (ord.filled_qty >= ord.initial_qty) {
                            ord.active = false;
                        }
                        return report;
                    }
                }
            }
        }
        report.exec_type = 8;
        return report;
    }

    int64_t evaluate_markout(size_t slot, int64_t current_mid_price) noexcept {
        if (slot >= MaxRestingOrders) return 0;
        auto& ord = resting_orders[slot];
        if (ord.filled_qty == 0) return 0;

        int64_t pnl = 0;
        if (ord.side == 1) {
            pnl = current_mid_price - ord.fill_price;
        } else {
            pnl = ord.fill_price - current_mid_price;
        }
        ord.markout_pnl = pnl;
        return pnl;
    }

    SbeExecutionReport process_order(const SbeOrderMessage& ord, uint64_t now_ns) noexcept {
        SbeExecutionReport report{};
        report.timestamp_ns = now_ns;
        report.client_order_id = ord.client_order_id;
        report.exec_id = exec_id_counter++;
        report.side = ord.side;

        if (ord.quantity == 0) {
            report.exec_type = 8;
            return report;
        }

        auto sweep_res = book.sweep(ord.side, ord.quantity);
        if (sweep_res.filled_quantity > 0) {
            report.exec_type = (sweep_res.unfilled_quantity == 0) ? 2 : 1;
            report.exec_price = sweep_res.vwap;
            report.exec_quantity = sweep_res.filled_quantity;
        } else {
            report.exec_type = 8;
        }
        return report;
    }
};

}
