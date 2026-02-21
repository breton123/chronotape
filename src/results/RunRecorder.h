#pragma once
#include "results/MetricsEngine.h"

class RunRecorder
{
public:
    explicit RunRecorder(MetricsConfig cfg) : metrics_(cfg) {}

    void reset() { metrics_.reset(); }
    void reserve(size_t bars, size_t trades_guess = 0) { metrics_.reserve(bars, trades_guess); }
    void finalize() { metrics_.finalize(); }

    // per bar
    inline void on_bar(int64_t ts, float balance, float equity, float unrealized, bool in_market)
    {
        metrics_.on_bar(ts, balance, equity, unrealized, in_market);
    }

    // trade close
    inline void on_trade_closed(const ClosedTrade &t)
    {
        metrics_.on_trade_closed(t);
    }

    const RunSeries &series() const { return metrics_.series(); }
    const TradeLog &trades() const { return metrics_.trades(); }

private:
    MetricsEngine metrics_;
};
