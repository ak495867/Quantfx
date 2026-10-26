#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

namespace qfx {

struct VaRResult {
    int64_t var_95 = 0;
    int64_t var_99 = 0;
    int64_t cvar_99 = 0;
    size_t sample_count = 0;
};

template <size_t WindowSize = 500>
class StreamingVaR {
public:
    std::vector<int64_t> returns;
    size_t write_pos = 0;

    StreamingVaR() {
        returns.reserve(WindowSize);
    }

    void push_return(int64_t pnl_diff) noexcept {
        if (returns.size() < WindowSize) {
            returns.push_back(pnl_diff);
        } else {
            returns[write_pos % WindowSize] = pnl_diff;
            write_pos++;
        }
    }

    VaRResult calculate() const {
        VaRResult res{};
        res.sample_count = returns.size();
        if (returns.empty()) return res;

        std::vector<int64_t> sorted = returns;
        std::sort(sorted.begin(), sorted.end());

        size_t idx_95 = static_cast<size_t>(std::floor(sorted.size() * 0.05));
        size_t idx_99 = static_cast<size_t>(std::floor(sorted.size() * 0.01));

        if (idx_95 >= sorted.size()) idx_95 = sorted.size() - 1;
        if (idx_99 >= sorted.size()) idx_99 = sorted.size() - 1;

        res.var_95 = -sorted[idx_95];
        res.var_99 = -sorted[idx_99];

        __int128 cvar_sum = 0;
        size_t tail_count = 0;
        for (size_t i = 0; i <= idx_99; ++i) {
            cvar_sum += sorted[i];
            tail_count++;
        }
        if (tail_count > 0) {
            res.cvar_99 = -static_cast<int64_t>(cvar_sum / tail_count);
        }
        return res;
    }
};

}
