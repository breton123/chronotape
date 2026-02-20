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
        float balance() const { return balance_; }
        float equity() const { return equity_; }
        float unrealized_pnl() const { return unrealized_pnl_; }
        float position_lots() const { return position_lots_; } // +long, -short
        float avg_entry() const { return avg_entry_; }         // entry price for net pos; NaN if flat

        const std::vector<Fill> &fills() const { return fills_; }

    private:
        uint64_t exec(Side side, int64_t ts, float mid_price, float lots);

        // price helpers
        float half_spread_price() const;
        float slippage_price() const;

        // netting helpers
        void apply_fill(Side side, int64_t ts, float fill_price, float lots, float commission);

        SymbolSpec spec_;
        CostsModel costs_;

        float balance_ = 0.0f;
        float equity_ = 0.0f;
        float unrealized_pnl_ = 0.0f;

        float position_lots_ = 0.0f; // +long, -short
        float avg_entry_ = NAN;      // valid if position_lots_ != 0

        float last_mid_ = NAN;
        uint64_t next_fill_id_ = 1;
        std::vector<Fill> fills_;
    };

} // namespace broker
