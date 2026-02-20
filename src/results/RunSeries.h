#pragma once
#include <cstdint>
#include <vector>
#include <cmath>

struct RunSeries
{
    // time axis (needed for chart alignment)
    std::vector<int64_t> ts;

    // ---- per-bar metrics you listed ----
    std::vector<float> balance;
    std::vector<float> equity;
    std::vector<float> dd_equity;
    std::vector<float> dd_balance;

    std::vector<float> avg_equity_dd;  // running avg drawdown (equity)
    std::vector<float> avg_balance_dd; // running avg drawdown (balance)

    std::vector<float> pct_in_equity_drawdown;
    std::vector<float> pct_in_balance_drawdown;
    std::vector<int32_t> bars_in_equity_drawdown;
    std::vector<int32_t> bars_in_balance_drawdown;

    std::vector<float> unrealized_pnl;
    std::vector<float> max_equity;
    std::vector<float> max_balance;
    std::vector<float> max_equity_dd;
    std::vector<float> max_balance_dd;

    std::vector<float> max_equity_daily_dd;
    std::vector<float> max_balance_daily_dd;

    std::vector<float> net_profit;

    // Trade counters/aggregates (these are “as of this bar”)
    std::vector<int32_t> total_trades;
    std::vector<int32_t> winning_trades;
    std::vector<int32_t> losing_trades;

    std::vector<float> win_rate;
    std::vector<float> gross_profit;
    std::vector<float> gross_loss;
    std::vector<float> profit_factor;

    std::vector<float> expected_value;
    std::vector<float> avg_win;
    std::vector<float> avg_loss;
    std::vector<float> profit_loss_ratio;

    std::vector<float> expectancy_r;
    std::vector<float> median_pnl;
    std::vector<float> top_10_percent_contribution;
    std::vector<float> trades_per_day;

    std::vector<float> time_in_market;

    std::vector<float> return_volatility;
    std::vector<float> sharpe_ratio;
    std::vector<float> calmar_ratio;
    std::vector<float> sortino_ratio;

    void reserve(size_t n)
    {
        ts.reserve(n);
        balance.reserve(n);
        equity.reserve(n);
        dd_equity.reserve(n);
        dd_balance.reserve(n);
        avg_equity_dd.reserve(n);
        avg_balance_dd.reserve(n);
        pct_in_equity_drawdown.reserve(n);
        pct_in_balance_drawdown.reserve(n);
        bars_in_equity_drawdown.reserve(n);
        bars_in_balance_drawdown.reserve(n);
        unrealized_pnl.reserve(n);
        max_equity.reserve(n);
        max_balance.reserve(n);
        max_equity_dd.reserve(n);
        max_balance_dd.reserve(n);
        max_equity_daily_dd.reserve(n);
        max_balance_daily_dd.reserve(n);
        net_profit.reserve(n);

        total_trades.reserve(n);
        winning_trades.reserve(n);
        losing_trades.reserve(n);
        win_rate.reserve(n);
        gross_profit.reserve(n);
        gross_loss.reserve(n);
        profit_factor.reserve(n);

        expected_value.reserve(n);
        avg_win.reserve(n);
        avg_loss.reserve(n);
        profit_loss_ratio.reserve(n);

        expectancy_r.reserve(n);
        median_pnl.reserve(n);
        top_10_percent_contribution.reserve(n);
        trades_per_day.reserve(n);

        time_in_market.reserve(n);
        return_volatility.reserve(n);
        sharpe_ratio.reserve(n);
        calmar_ratio.reserve(n);
        sortino_ratio.reserve(n);
    }

    size_t size() const { return ts.size(); }

    void clear()
    {
        *this = RunSeries{};
    }
};
