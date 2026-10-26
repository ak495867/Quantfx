#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <cstring>

namespace qfx {

struct alignas(2) SbeHeader {
    uint16_t block_length = sizeof(Order);
    uint16_t template_id = 101;
    uint16_t schema_id = 1;
    uint16_t version = 1;
};

struct alignas(8) SbeOrderMessage {
    SbeHeader header;
    uint64_t timestamp_ns = 0;
    uint64_t client_order_id = 0;
    uint64_t symbol = 0;
    int64_t price = 0;
    uint64_t quantity = 0;
    int8_t side = 0;
    uint8_t time_in_force = 0;
    uint16_t padding = 0;
    uint32_t reserved = 0;
};

static_assert(sizeof(SbeOrderMessage) == 56, "SbeOrderMessage must be exactly 56 bytes");

inline size_t encode_sbe_order(uint8_t* out_buf, size_t buf_len, const Order& ord, uint64_t ts_ns) noexcept {
    if (buf_len < sizeof(SbeOrderMessage)) return 0;
    auto* msg = reinterpret_cast<SbeOrderMessage*>(out_buf);
    msg->header = SbeHeader{};
    msg->timestamp_ns = ts_ns;
    msg->client_order_id = ord.client_order_id;
    msg->symbol = ord.symbol;
    msg->price = ord.price;
    msg->quantity = ord.quantity;
    msg->side = static_cast<int8_t>(ord.side);
    msg->time_in_force = 0;
    msg->padding = 0;
    msg->reserved = 0;
    return sizeof(SbeOrderMessage);
}

inline bool decode_sbe_order(const uint8_t* in_buf, size_t buf_len, Order& ord, uint64_t& ts_ns) noexcept {
    if (buf_len < sizeof(SbeOrderMessage)) return false;
    const auto* msg = reinterpret_cast<const SbeOrderMessage*>(in_buf);
    if (msg->header.template_id != 101) return false;
    ts_ns = msg->timestamp_ns;
    ord.client_order_id = msg->client_order_id;
    ord.symbol = msg->symbol;
    ord.price = msg->price;
    ord.quantity = msg->quantity;
    ord.side = msg->side;
    ord.stop_loss = 0;
    ord.take_profit = 0;
    return true;
}

}
