#pragma once

#include <cstdint>
#include <vector>
#include <cmath>
#include <numeric>

namespace qfx {

struct InstitutionalStats {
    double total_return_pct = 0.0;
    double annualized_return_pct = 0.0;
    double annual_volatility_pct = 0.0;
    double sharpe_ratio = 0.0;
    double sortino_ratio = 0.0;
    double calmar_ratio = 0.0;
    double profit_factor = 0.0;
    double max_drawdown_pct = 0.0;
    uint64_t trade_count = 0;
};

class QuantAnalytics {
public:
    std::vector<double> period_returns;

    void add_return(double ret) noexcept {
        period_returns.push_back(ret);
    }

    void reset() noexcept {
        period_returns.clear();
    }

    InstitutionalStats compute(
        int64_t initial_cash,
        int64_t final_equity,
        int64_t max_drawdown,
        int64_t gross_profit,
        int64_t gross_loss,
        uint64_t total_trades,
        double periods_per_year = 252.0
    ) const {
        InstitutionalStats stats{};
        stats.trade_count = total_trades;

        if (initial_cash <= 0) return stats;

        double ret = static_cast<double>(final_equity - initial_cash) / static_cast<double>(initial_cash);
        stats.total_return_pct = ret * 100.0;
        stats.max_drawdown_pct = (static_cast<double>(max_drawdown) / static_cast<double>(initial_cash)) * 100.0;

        if (gross_loss > 0) {
            stats.profit_factor = static_cast<double>(gross_profit) / static_cast<double>(gross_loss);
        } else if (gross_profit > 0) {
            stats.profit_factor = 999.0;
        }

        if (period_returns.size() < 2) {
            stats.annualized_return_pct = stats.total_return_pct;
            return stats;
        }

        double sum = std::accumulate(period_returns.begin(), period_returns.end(), 0.0);
        double mean = sum / period_returns.size();
        stats.annualized_return_pct = mean * periods_per_year * 100.0;

        double sq_sum = 0.0;
        double downside_sq_sum = 0.0;
        for (double r : period_returns) {
            double diff = r - mean;
            sq_sum += diff * diff;
            if (r < 0.0) {
                downside_sq_sum += r * r;
            }
        }

        double variance = sq_sum / (period_returns.size() - 1);
        double stdev = std::sqrt(variance);
        double annual_stdev = stdev * std::sqrt(periods_per_year);
        stats.annual_volatility_pct = annual_stdev * 100.0;

        if (annual_stdev > 1e-9) {
            stats.sharpe_ratio = (mean * periods_per_year) / annual_stdev;
        }

        double downside_stdev = std::sqrt(downside_sq_sum / period_returns.size()) * std::sqrt(periods_per_year);
        if (downside_stdev > 1e-9) {
            stats.sortino_ratio = (mean * periods_per_year) / downside_stdev;
        }

        if (stats.max_drawdown_pct > 1e-9) {
            stats.calmar_ratio = stats.annualized_return_pct / stats.max_drawdown_pct;
        }

        return stats;
    }
};

}
