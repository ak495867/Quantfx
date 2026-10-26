#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

namespace qfx {

enum class FeedPacketAction : uint8_t {
    ACCEPT = 0,
    DUPLICATE_DROP = 1,
    GAP_DETECTED = 2,
    WINDOW_OVERFLOW = 3
};

struct FeedPacketResult {
    FeedPacketAction action;
    uint8_t feed_id;
    uint64_t sequence;
    uint64_t next_expected;
};

template <size_t WindowSize = 1024>
class FeedArbitrator {
private:
    uint64_t next_expected_seq_ = 1;
    std::array<bool, WindowSize> received_window_{};
    uint64_t window_base_seq_ = 1;

public:
    uint64_t accepted_a = 0;
    uint64_t accepted_b = 0;
    uint64_t duplicate_drops = 0;
    uint64_t gap_count = 0;

    void reset(uint64_t initial_seq = 1) noexcept {
        next_expected_seq_ = initial_seq;
        window_base_seq_ = initial_seq;
        received_window_.fill(false);
        accepted_a = 0;
        accepted_b = 0;
        duplicate_drops = 0;
        gap_count = 0;
    }

    FeedPacketResult process_packet(uint8_t feed_id, uint64_t sequence) noexcept {
        FeedPacketResult res{};
        res.feed_id = feed_id;
        res.sequence = sequence;

        if (sequence < next_expected_seq_) {
            duplicate_drops++;
            res.action = FeedPacketAction::DUPLICATE_DROP;
            res.next_expected = next_expected_seq_;
            return res;
        }

        if (sequence == next_expected_seq_) {
            if (feed_id == 0) accepted_a++; else accepted_b++;
            next_expected_seq_++;

            size_t slot = (sequence - window_base_seq_) % WindowSize;
            received_window_[slot] = false;

            while (window_base_seq_ < next_expected_seq_) {
                size_t base_slot = window_base_seq_ % WindowSize;
                received_window_[base_slot] = false;
                window_base_seq_++;
            }

            while (true) {
                uint64_t diff = next_expected_seq_ - window_base_seq_;
                if (diff >= WindowSize) break;
                size_t lookahead_slot = next_expected_seq_ % WindowSize;
                if (!received_window_[lookahead_slot]) break;
                received_window_[lookahead_slot] = false;
                next_expected_seq_++;
            }

            res.action = FeedPacketAction::ACCEPT;
            res.next_expected = next_expected_seq_;
            return res;
        }

        uint64_t gap = sequence - next_expected_seq_;
        if (gap >= WindowSize) {
            res.action = FeedPacketAction::WINDOW_OVERFLOW;
            res.next_expected = next_expected_seq_;
            return res;
        }

        size_t slot = sequence % WindowSize;
        if (received_window_[slot]) {
            duplicate_drops++;
            res.action = FeedPacketAction::DUPLICATE_DROP;
            res.next_expected = next_expected_seq_;
            return res;
        }

        received_window_[slot] = true;
        gap_count++;
        if (feed_id == 0) accepted_a++; else accepted_b++;

        res.action = FeedPacketAction::GAP_DETECTED;
        res.next_expected = next_expected_seq_;
        return res;
    }

    uint64_t next_expected() const noexcept {
        return next_expected_seq_;
    }
};

}
