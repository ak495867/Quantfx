#pragma once

#include "qfx.hpp"
#include "qfx_l2.hpp"
#include "qfx_sbe.hpp"
#include <cstdint>
#include <algorithm>

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

class VenueSimulator {
public:
    L2OrderBook<10> book;
    uint64_t exec_id_counter = 1;

    void seed_default_market(int64_t mid = 108000) noexcept {
        book.clear();
        book.update_bid(0, mid - 5, 500000);
        book.update_bid(1, mid - 10, 1000000);
        book.update_bid(2, mid - 15, 2000000);
        book.update_ask(0, mid + 5, 500000);
        book.update_ask(1, mid + 10, 1000000);
        book.update_ask(2, mid + 15, 2000000);
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
