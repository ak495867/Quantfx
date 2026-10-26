#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <sstream>

namespace qfx {

struct LatencyStats {
    uint64_t count = 0;
    uint64_t min_ns = 0;
    uint64_t p50_ns = 0;
    uint64_t p90_ns = 0;
    uint64_t p99_ns = 0;
    uint64_t p999_ns = 0;
    uint64_t max_ns = 0;
    double mean_ns = 0.0;
};

class LatencyHistogram {
public:
    std::vector<uint64_t> samples;

    void reserve(size_t n) {
        samples.reserve(n);
    }

    void record_ns(uint64_t ns) noexcept {
        samples.push_back(ns);
    }

    void reset() noexcept {
        samples.clear();
    }

    LatencyStats compute() {
        LatencyStats stats{};
        stats.count = samples.size();
        if (samples.empty()) return stats;

        std::sort(samples.begin(), samples.end());

        stats.min_ns = samples.front();
        stats.max_ns = samples.back();

        size_t idx_50 = static_cast<size_t>(samples.size() * 0.50);
        size_t idx_90 = static_cast<size_t>(samples.size() * 0.90);
        size_t idx_99 = static_cast<size_t>(samples.size() * 0.99);
        size_t idx_999 = static_cast<size_t>(samples.size() * 0.999);

        stats.p50_ns = samples[std::min(idx_50, samples.size() - 1)];
        stats.p90_ns = samples[std::min(idx_90, samples.size() - 1)];
        stats.p99_ns = samples[std::min(idx_99, samples.size() - 1)];
        stats.p999_ns = samples[std::min(idx_999, samples.size() - 1)];

        uint64_t total = std::accumulate(samples.begin(), samples.end(), 0ULL);
        stats.mean_ns = static_cast<double>(total) / static_cast<double>(samples.size());

        return stats;
    }
};

}
