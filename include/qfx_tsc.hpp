#pragma once

#include <cstdint>
#include <chrono>
#include <thread>
#include <x86intrin.h>

namespace qfx {

class TscClock {
public:
    uint64_t tsc_base = 0;
    uint64_t ns_base = 0;
    double ns_per_tick = 1.0;
    bool calibrated = false;

    void calibrate(uint32_t sample_ms = 10) noexcept {
        auto t0_wall = std::chrono::steady_clock::now();
        uint64_t t0_tsc = __rdtsc();

        std::this_thread::sleep_for(std::chrono::milliseconds(sample_ms));

        auto t1_wall = std::chrono::steady_clock::now();
        uint64_t t1_tsc = __rdtsc();

        uint64_t elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1_wall - t0_wall).count();
        uint64_t elapsed_tsc = t1_tsc - t0_tsc;

        if (elapsed_tsc > 0) {
            ns_per_tick = static_cast<double>(elapsed_ns) / static_cast<double>(elapsed_tsc);
        }

        tsc_base = t1_tsc;
        ns_base = std::chrono::duration_cast<std::chrono::nanoseconds>(t1_wall.time_since_epoch()).count();
        calibrated = true;
    }

    inline uint64_t now_ns() const noexcept {
        if (!calibrated) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }
        uint64_t current_tsc = __rdtsc();
        uint64_t diff = (current_tsc >= tsc_base) ? (current_tsc - tsc_base) : 0;
        return ns_base + static_cast<uint64_t>(diff * ns_per_tick);
    }
};

}
