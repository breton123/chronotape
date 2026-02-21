// src/broker/BrokerSim.h
#pragma once
#include <cstdint>
#include <vector>
#include <cmath>

namespace broker
{

    // FX-only, netting model (single net position per symbol).
    // Prices are treated as "mid" prices coming from your tape (e.g., bar.close).
    // Fills apply spread/2 + slippage on top of mid.

    enum class Side
    {
        Buy,
        Sell
    };

    struct ClosedTrade
    {
        int64_t entry_ts = 0;
        int64_t exit_ts = 0;
        int32_t entry_i = -1;
        int32_t exit_i = -1;

        float entry_price = NAN;
        float exit_price = NAN;

        float lots_closed = 0.0f; // magnitude
        Side side = Side::Buy;    // side of the ORIGINAL position (Buy=long, Sell=short)

        float realized_pnl = 0.0f; // currency
        float commission = 0.0f;   // commission on the closing fill (optional)

        int hold_bars = 0; // optional, can compute from entry/exit i
    };

    using OnClosedTradeFn = void (*)(void *user, const ClosedTrade &t);

    struct CostsModel
    {
        float spread_pips = 0.8f;        // fixed spread in pips (FX)
        float slippage_pips = 0.0f;      // fixed slippage in pips
        float commission_per_lot = 0.0f; // per 1.0 lot, charged on each fill
    };

    struct SymbolSpec
    {
        float pip_size = 0.0001f;   // EURUSD 0.0001, USDJPY 0.01, etc.
        float lot_size = 100000.0f; // standard FX lot = 100k base units
    };

    struct Fill
    {
        uint64_t id = 0;
        int64_t ts = 0;
        Side side = Side::Buy;
        float lots = 0.0f;
        float price = NAN;         // executed price (mid +/- costs)
        float commission = 0.0f;   // commission charged for this fill
        float realized_pnl = 0.0f; // pnl realized by this fill (if it reduces/flip)
    };

    struct Metrics
    {
        int bars = 0;
        float balance = 0.0f;    // done
        float equity = 0.0f;     // done
        float dd_equity = 0.0f;  // done
        float dd_balance = 0.0f; // done
        float avg_equity_dd = 0.0f;
        float avg_balance_dd = 0.0f;
        float pct_in_equity_drawdown = 0.0f;  // done
        float pct_in_balance_drawdown = 0.0f; // done
        int bars_in_equity_drawdown = 0;
        int bars_in_balance_drawdown = 0;
        float unrealized_pnl = 0.0f; // done
        float max_equity = 0.0f;     // done
        float max_balance = 0.0f;    // done
        float max_equity_dd = 0.0f;  // done
        float max_balance_dd = 0.0f; // done
        float max_equity_daily_dd = 0.0f;
        float max_balance_daily_dd = 0.0f;
        float net_profit = 0.0f;
        int total_trades = 0;
        int winning_trades = 0;
        int losting_trades = 0;
        float win_rate = 0.0f;
        float gross_profit = 0.0f;
        float gross_loss = 0.0f;
        float profit_factor = 0.0f;
        float expected_value = 0.0f;
        float avg_win = 0.0f;
        float avg_loss = 0.0f;
        float profit_loss_ratio = 0.0f;
        float expectancy_r = 0.0f;
        float median_pnl = 0.0f;
        float top_10_percent_contribution = 0.0f;
        float trades_per_day = 0.0f;
        float avg_hold_bars = 0.0f;
        float time_in_market = 0.0f;
        float total_costs = 0.0f;
        float cost_pct = 0.0f;
        float return_volatility = 0.0f;
        float sharpe_ratio = 0.0f;
        float calmar_ratio = 0.0f;
        float sortino_ratio = 0.0f;
        float no_trade_rate = 0.0f;
    };

    class BrokerSim
    {
    public:
        BrokerSim(SymbolSpec spec, CostsModel costs, float initial_balance);

        // Update the broker each bar so equity/unrealized are current.
        // mid_price is typically bar.close (your tape close).
        void on_bar(int64_t ts, float mid_price);

        // Market orders (lots > 0). Returns fill id.
        uint64_t buy_market(int64_t ts, float mid_price, float lots);
        uint64_t sell_market(int64_t ts, float mid_price, float lots);

        // Close any open net position at market. Returns fill id or 0 if flat.
        uint64_t close_all(int64_t ts, float mid_price);

        // State
        bool account_blown() const { return account_blown_; }
        float balance() const { return balance_; }
        float equity() const { return equity_; }
        float unrealized_pnl() const { return unrealized_pnl_; }
        float position_lots() const { return position_lots_; } // +long, -short
        float avg_entry() const { return avg_entry_; }         // entry price for net pos; NaN if flat
        Metrics metrics() const { return Metrics{bars_, balance_, equity_, dd_equity_, dd_balance_, avg_equity_dd_, avg_balance_dd_, pct_in_equity_drawdown_, pct_in_balance_drawdown_, bars_in_equity_drawdown_, bars_in_balance_drawdown_, unrealized_pnl_, max_equity_, max_balance_, max_equity_dd_, max_balance_dd_, max_equity_daily_dd_, max_balance_daily_dd_, net_profit_, total_trades_, winning_trades_, losting_trades_, win_rate_, gross_profit_, gross_loss_, profit_factor_, expected_value_, avg_win_, avg_loss_, profit_loss_ratio_, expectancy_r_, median_pnl_, top_10_percent_contribution_, trades_per_day_, avg_hold_bars_, time_in_market_, total_costs_, cost_pct_, return_volatility_, sharpe_ratio_, calmar_ratio_, sortino_ratio_, no_trade_rate_}; }

        const std::vector<Fill> &fills() const { return fills_; }

        void set_on_closed_trade(OnClosedTradeFn fn, void *user)
        {
            on_closed_trade_ = fn;
            on_closed_trade_user_ = user;
        }

        void set_bar_index(int32_t i) { bar_index_ = i; }

    private:
        uint64_t exec(Side side, int64_t ts, float mid_price, float lots);

        // price helpers
        float half_spread_price() const;
        float slippage_price() const;

        OnClosedTradeFn on_closed_trade_ = nullptr;
        void *on_closed_trade_user_ = nullptr;

        // Track current open position entry metadata
        int64_t entry_ts_ = 0;
        int32_t entry_i_ = -1;

        int32_t bar_index_ = -1;

        // netting helpers
        void apply_fill(Side side, int64_t ts, float fill_price, float lots, float commission);

        SymbolSpec spec_;
        CostsModel costs_;
        int bars_ = 0;
        float balance_ = 0.0f;    // done
        float equity_ = 0.0f;     // done
        float dd_equity_ = 0.0f;  // done
        float dd_balance_ = 0.0f; // done
        float avg_equity_dd_ = 0.0f;
        float avg_balance_dd_ = 0.0f;
        float pct_in_equity_drawdown_ = 0.0f;  // done
        float pct_in_balance_drawdown_ = 0.0f; // done
        int bars_in_equity_drawdown_ = 0;
        int bars_in_balance_drawdown_ = 0;
        float unrealized_pnl_ = 0.0f; // done
        float max_equity_ = 0.0f;     // done
        float max_balance_ = 0.0f;    // done
        float max_equity_dd_ = 0.0f;  // done
        float max_balance_dd_ = 0.0f; // done
        float max_equity_daily_dd_ = 0.0f;
        float max_balance_daily_dd_ = 0.0f;
        float net_profit_ = 0.0f;
        int total_trades_ = 0;
        int winning_trades_ = 0;
        int losting_trades_ = 0;
        float win_rate_ = 0.0f;
        float gross_profit_ = 0.0f;
        float gross_loss_ = 0.0f;
        float profit_factor_ = 0.0f;
        float expected_value_ = 0.0f;
        float avg_win_ = 0.0f;
        float avg_loss_ = 0.0f;
        float profit_loss_ratio_ = 0.0f;
        float expectancy_r_ = 0.0f;
        float median_pnl_ = 0.0f;
        float top_10_percent_contribution_ = 0.0f;
        float trades_per_day_ = 0.0f;
        float avg_hold_bars_ = 0.0f;
        float time_in_market_ = 0.0f;
        float total_costs_ = 0.0f;
        float cost_pct_ = 0.0f;
        float return_volatility_ = 0.0f;
        float sharpe_ratio_ = 0.0f;
        float calmar_ratio_ = 0.0f;
        float sortino_ratio_ = 0.0f;
        float no_trade_rate_ = 0.0f;
        bool account_blown_ = false;
        float position_lots_ = 0.0f; // +long, -short
        float avg_entry_ = NAN;      // valid if position_lots_ != 0

        float last_mid_ = NAN;
        uint64_t next_fill_id_ = 1;
        std::vector<Fill> fills_;
    };

} // namespace broker
