#pragma once
#include "RunSeries.h"
#include "TradeLog.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

struct MetricsConfig
{
    float initial_equity = 100000.0f;
    int annualization_bars = 252 * 24 * 60; // M1 default; change per timeframe
};

class MetricsEngine
{
public:
    explicit MetricsEngine(MetricsConfig cfg) : cfg_(cfg) {}

    void reset();
    void reserve(size_t bars, size_t trades_guess = 0);
    void finalize();

    // Call when a trade closes
    void on_trade_closed(const ClosedTrade &t);

    // Call once per bar
    void on_bar(int64_t ts,
                float balance,
                float equity,
                float unrealized_pnl,
                bool in_market);

    const RunSeries &series() const { return series_; }
    const TradeLog &trades() const { return trades_; }

private:
    // Helpers
    void update_drawdown(float equity, float balance);
    void update_daily_dd(int64_t ts, float equity, float balance);
    void update_trade_aggregates();
    void update_return_stats(float equity);
    void update_tail_stats(); // median/top10 contribution (called from finalize)
    float safe_div(float a, float b) const { return (b != 0.0f) ? (a / b) : NAN; }

    MetricsConfig cfg_;
    RunSeries series_;
    TradeLog trades_;

    // Running state (not per-bar vectors)
    float eq0_ = NAN;

    float max_equity_ = -INFINITY;
    float max_balance_ = -INFINITY;

    float max_equity_dd_ = 0.0f; // negative
    float max_balance_dd_ = 0.0f;

    double sum_equity_dd_ = 0.0; // negative sum
    double sum_balance_dd_ = 0.0;
    int bars_in_equity_dd_ = 0;
    int bars_in_balance_dd_ = 0;

    // Daily DD tracking
    int current_day_key_ = -1; // YYYYMMDD
    float day_start_equity_ = NAN;
    float day_start_balance_ = NAN;
    float max_equity_daily_dd_ = 0.0f; // negative
    float max_balance_daily_dd_ = 0.0f;

    // Trade aggregates
    int total_trades_ = 0;
    int wins_ = 0;
    int losses_ = 0;
    double gross_profit_ = 0.0;
    double gross_loss_ = 0.0; // positive magnitude
    double sum_win_ = 0.0;
    double sum_loss_ = 0.0; // positive magnitude

    // For median/top10% contribution
    std::vector<float> closed_pnls_; // store all trade pnls (can be large but manageable)

    // Trades-per-day
    int64_t first_ts_ = 0;
    int64_t last_ts_ = 0;

    // Return stats (for Sharpe/vol)
    bool have_prev_eq_ = false;
    float prev_eq_ = NAN;
    // Welford for mean/std of log returns
    int ret_n_ = 0;
    double ret_mean_ = 0.0;
    double ret_M2_ = 0.0;

    // Downside returns for Sortino
    int down_n_ = 0;
    double down_M2_ = 0.0; // variance accumulator of negative returns
};
