#include "results/MetricsEngine.h"

void MetricsEngine::reset()
{
    series_.clear();
    trades_ = TradeLog{};
    closed_pnls_.clear();

    eq0_ = NAN;
    max_equity_ = -INFINITY;
    max_balance_ = -INFINITY;
    max_equity_dd_ = 0.0f;
    max_balance_dd_ = 0.0f;
    sum_equity_dd_ = 0.0;
    sum_balance_dd_ = 0.0;
    bars_in_equity_dd_ = 0;
    bars_in_balance_dd_ = 0;

    current_day_key_ = -1;
    day_start_equity_ = NAN;
    day_start_balance_ = NAN;
    max_equity_daily_dd_ = 0.0f;
    max_balance_daily_dd_ = 0.0f;

    total_trades_ = wins_ = losses_ = 0;
    gross_profit_ = gross_loss_ = 0.0;
    sum_win_ = sum_loss_ = 0.0;

    first_ts_ = last_ts_ = 0;

    have_prev_eq_ = false;
    prev_eq_ = NAN;
    ret_n_ = 0;
    ret_mean_ = 0.0;
    ret_M2_ = 0.0;
    down_n_ = 0;
    down_M2_ = 0.0;
}

void MetricsEngine::reserve(size_t bars, size_t trades_guess)
{
    series_.reserve(bars);
    trades_.reserve(trades_guess);
    closed_pnls_.reserve(trades_guess);
}

void MetricsEngine::on_trade_closed(const ClosedTrade &t)
{
    trades_.add_closed(t);
    closed_pnls_.push_back(t.pnl);

    total_trades_++;

    if (t.pnl > 0.0f)
    {
        wins_++;
        gross_profit_ += t.pnl;
        sum_win_ += t.pnl;
    }
    else if (t.pnl < 0.0f)
    {
        losses_++;
        const float loss_mag = -t.pnl;
        gross_loss_ += loss_mag;
        sum_loss_ += loss_mag;
    }

    // tail stats can be computed at end or periodically; we’ll compute per bar cheaply using nth_element on copy (later)
}

void MetricsEngine::on_bar(int64_t ts, float balance, float equity, float unrealized_pnl, bool in_market)
{
    if (series_.ts.empty())
    {
        eq0_ = std::isnan(eq0_) ? cfg_.initial_equity : eq0_;
        first_ts_ = ts;
    }
    last_ts_ = ts;

    update_drawdown(equity, balance);
    update_daily_dd(ts, equity, balance);
    update_return_stats(equity);

    // --- compute per-bar “as of now” aggregates ---
    const float net_profit = equity - (std::isnan(eq0_) ? equity : eq0_);

    const float win_rate = (total_trades_ > 0) ? (float)wins_ / (float)total_trades_ : NAN;
    const float avg_win = (wins_ > 0) ? (float)(sum_win_ / (double)wins_) : NAN;
    const float avg_loss = (losses_ > 0) ? (float)(sum_loss_ / (double)losses_) : NAN;

    const float pf = (gross_loss_ > 0.0) ? (float)(gross_profit_ / gross_loss_) : (gross_profit_ > 0.0 ? INFINITY : NAN);

    const float expectancy = (!std::isnan(win_rate) && !std::isnan(avg_win) && !std::isnan(avg_loss))
                                 ? (win_rate * avg_win) - ((1.0f - win_rate) * avg_loss)
                                 : NAN;

    const float pl_ratio = (!std::isnan(avg_win) && !std::isnan(avg_loss) && avg_loss > 0.0f)
                               ? (avg_win / avg_loss)
                               : NAN;

    // trades/day (based on elapsed days in data)
    float trades_per_day = NAN;
    if (first_ts_ != 0 && last_ts_ > first_ts_)
    {
        const double days = (double)(last_ts_ - first_ts_) / (1000.0 * 60.0 * 60.0 * 24.0);
        if (days > 0.0)
            trades_per_day = (float)((double)total_trades_ / days);
    }

    // time in market (running fraction)
    static int bars_seen = 0;
    static int bars_in_mkt = 0;
    bars_seen++;
    if (in_market)
        bars_in_mkt++;
    const float time_in_market = (bars_seen > 0) ? (float)bars_in_mkt / (float)bars_seen : 0.0f;

    // median + top10% contribution (expensive if done per bar exactly)
    // For v1: update every N bars or at end. Here we do a cheap “mostly-up-to-date” update every 500 bars.
    float median_pnl = NAN;
    float top10_contrib = NAN;
    if (!closed_pnls_.empty())
    {
        if ((series_.ts.size() % 500) == 0 || series_.ts.size() < 10)
        {
            // median
            auto tmp = closed_pnls_;
            size_t mid = tmp.size() / 2;
            std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end());
            median_pnl = tmp[mid];

            // top 10% contribution to gross profit
            std::sort(tmp.begin(), tmp.end(), std::greater<float>());
            const size_t k = std::max<size_t>(1, (size_t)std::ceil(tmp.size() * 0.10));
            double top_sum = 0.0;
            for (size_t j = 0; j < k; ++j)
                if (tmp[j] > 0.0f)
                    top_sum += tmp[j];
            top10_contrib = (gross_profit_ > 0.0) ? (float)(top_sum / gross_profit_) : NAN;
        }
        else
        {
            // carry forward last computed values if available
            if (!series_.median_pnl.empty())
                median_pnl = series_.median_pnl.back();
            if (!series_.top_10_percent_contribution.empty())
                top10_contrib = series_.top_10_percent_contribution.back();
        }
    }

    // volatility/sharpe/sortino (bar-based on log returns)
    float vol = NAN, sharpe = NAN, sortino = NAN;
    if (ret_n_ > 1)
    {
        const double var = ret_M2_ / (double)(ret_n_ - 1);
        vol = (float)std::sqrt(std::max(0.0, var));
        if (vol > 0.0f)
        {
            const double mean = ret_mean_;
            sharpe = (float)(mean / (double)vol * std::sqrt((double)cfg_.annualization_bars));
        }
        if (down_n_ > 1)
        {
            const double dvar = down_M2_ / (double)(down_n_ - 1);
            const double dstd = std::sqrt(std::max(0.0, dvar));
            if (dstd > 0.0)
            {
                sortino = (float)(ret_mean_ / dstd * std::sqrt((double)cfg_.annualization_bars));
            }
        }
    }

    // calmar = annualized return / |max dd|  (max dd in currency -> convert to pct using peak equity)
    float calmar = NAN;
    if (!series_.equity.empty())
    {
        // approximate annualized return from bar frequency
        const double total_ret = (equity / series_.equity.front()) - 1.0;
        const double years = ((double)(last_ts_ - first_ts_)) / (1000.0 * 60.0 * 60.0 * 24.0 * 365.0);
        if (years > 0.0 && !std::isnan(series_.max_equity_dd.back()))
        {
            const double ann = std::pow(1.0 + total_ret, 1.0 / years) - 1.0;
            // use max_dd_pct (negative) if peak known; we store max_equity_dd_pct indirectly not yet, so compute from peak now:
            // We’ll estimate max dd pct as max_equity_dd_ / max_equity_ (both tracked)
            if (max_equity_ > 0.0f)
            {
                const double maxddpct = (double)max_equity_dd_ / (double)max_equity_; // negative
                if (maxddpct < 0.0)
                    calmar = (float)(ann / std::fabs(maxddpct));
            }
        }
    }

    // --- append to SoA series ---
    series_.ts.push_back(ts);

    series_.balance.push_back(balance);
    series_.equity.push_back(equity);
    series_.dd_equity.push_back(series_.equity.back() - max_equity_);
    series_.dd_balance.push_back(series_.balance.back() - max_balance_);

    series_.avg_equity_dd.push_back((bars_in_equity_dd_ > 0) ? (float)(sum_equity_dd_ / (double)bars_in_equity_dd_) : 0.0f);
    series_.avg_balance_dd.push_back((bars_in_balance_dd_ > 0) ? (float)(sum_balance_dd_ / (double)bars_in_balance_dd_) : 0.0f);

    series_.pct_in_equity_drawdown.push_back(series_.ts.size() > 0 ? (float)bars_in_equity_dd_ / (float)series_.ts.size() : 0.0f);
    series_.pct_in_balance_drawdown.push_back(series_.ts.size() > 0 ? (float)bars_in_balance_dd_ / (float)series_.ts.size() : 0.0f);

    series_.bars_in_equity_drawdown.push_back(bars_in_equity_dd_);
    series_.bars_in_balance_drawdown.push_back(bars_in_balance_dd_);

    series_.unrealized_pnl.push_back(unrealized_pnl);
    series_.max_equity.push_back(max_equity_);
    series_.max_balance.push_back(max_balance_);
    series_.max_equity_dd.push_back(max_equity_dd_);
    series_.max_balance_dd.push_back(max_balance_dd_);
    series_.max_equity_daily_dd.push_back(max_equity_daily_dd_);
    series_.max_balance_daily_dd.push_back(max_balance_daily_dd_);

    series_.net_profit.push_back(net_profit);

    series_.total_trades.push_back(total_trades_);
    series_.winning_trades.push_back(wins_);
    series_.losing_trades.push_back(losses_);

    series_.win_rate.push_back(win_rate);
    series_.gross_profit.push_back((float)gross_profit_);
    series_.gross_loss.push_back((float)gross_loss_);
    series_.profit_factor.push_back(pf);

    series_.expected_value.push_back(expectancy);
    series_.avg_win.push_back(avg_win);
    series_.avg_loss.push_back(avg_loss);
    series_.profit_loss_ratio.push_back(pl_ratio);

    // expectancy_r: only meaningful if you provide pnl_r in ClosedTrade; for v1 keep NAN/0 or compute mean pnl_r
    series_.expectancy_r.push_back(NAN);

    series_.median_pnl.push_back(median_pnl);
    series_.top_10_percent_contribution.push_back(top10_contrib);
    series_.trades_per_day.push_back(trades_per_day);

    series_.time_in_market.push_back(time_in_market);

    series_.return_volatility.push_back(vol);
    series_.sharpe_ratio.push_back(sharpe);
    series_.calmar_ratio.push_back(calmar);
    series_.sortino_ratio.push_back(sortino);
}

void MetricsEngine::update_drawdown(float equity, float balance)
{
    if (eq0_ != eq0_)
        eq0_ = cfg_.initial_equity; // NaN guard

    if (!std::isfinite(max_equity_))
        max_equity_ = equity;
    if (!std::isfinite(max_balance_))
        max_balance_ = balance;

    if (equity > max_equity_)
        max_equity_ = equity;
    if (balance > max_balance_)
        max_balance_ = balance;

    const float dd_eq = equity - max_equity_; // <=0
    const float dd_bal = balance - max_balance_;

    if (dd_eq < 0.0f)
    {
        bars_in_equity_dd_++;
        sum_equity_dd_ += dd_eq;
    }
    if (dd_bal < 0.0f)
    {
        bars_in_balance_dd_++;
        sum_balance_dd_ += dd_bal;
    }

    if (dd_eq < max_equity_dd_)
        max_equity_dd_ = dd_eq;
    if (dd_bal < max_balance_dd_)
        max_balance_dd_ = dd_bal;
}

void MetricsEngine::update_daily_dd(int64_t ts, float equity, float balance)
{
    const int day = ts;
    if (day != current_day_key_)
    {
        current_day_key_ = day;
        day_start_equity_ = equity;
        day_start_balance_ = balance;
    }
    // daily dd relative to day start (<=0)
    const float d_eq = equity - day_start_equity_;
    const float d_bal = balance - day_start_balance_;
    if (d_eq < max_equity_daily_dd_)
        max_equity_daily_dd_ = d_eq;
    if (d_bal < max_balance_daily_dd_)
        max_balance_daily_dd_ = d_bal;
}

void MetricsEngine::update_return_stats(float equity)
{
    if (!have_prev_eq_)
    {
        prev_eq_ = equity;
        have_prev_eq_ = true;
        return;
    }
    if (prev_eq_ <= 0.0f || equity <= 0.0f)
    {
        prev_eq_ = equity;
        return;
    }
    const double r = std::log((double)equity / (double)prev_eq_);
    prev_eq_ = equity;

    // Welford update
    ret_n_++;
    const double delta = r - ret_mean_;
    ret_mean_ += delta / (double)ret_n_;
    ret_M2_ += delta * (r - ret_mean_);

    // downside (only negative returns)
    if (r < 0.0)
    {
        down_n_++;
        // Welford on negative returns around 0: use variance of negative returns
        // simplest: track variance of r (for r<0) around its mean; ok for v1
        static double down_mean = 0.0;
        const double d = r - down_mean;
        down_mean += d / (double)down_n_;
        down_M2_ += d * (r - down_mean);
    }
}
