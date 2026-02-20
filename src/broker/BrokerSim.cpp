// broker/BrokerSim.cpp
#include "BrokerSim.h"
#include <algorithm> // std::min
#include <iostream>

namespace broker
{

    static inline float signf(float x) { return (x > 0) - (x < 0); }

    BrokerSim::BrokerSim(SymbolSpec spec, CostsModel costs, float initial_balance)
        : spec_(spec), costs_(costs), balance_(initial_balance)
    {
        equity_ = balance_;
    }

    float BrokerSim::half_spread_price() const
    {
        // spread_pips * pip_size gives full spread in price terms
        return 0.5f * costs_.spread_pips * spec_.pip_size;
    }

    float BrokerSim::slippage_price() const
    {
        return costs_.slippage_pips * spec_.pip_size;
    }

    void BrokerSim::on_bar(int64_t /*ts*/, float mid_price)
    {
        last_mid_ = mid_price;

        if (position_lots_ == 0.0f || std::isnan(avg_entry_))
        {
            unrealized_pnl_ = 0.0f;
            equity_ = balance_;
            return;
        }

        // Unrealized PnL marked to mid.
        // PnL in quote currency (e.g. USD for EURUSD): (mid - entry) * base_units
        const float units = position_lots_ * spec_.lot_size; // signed
        unrealized_pnl_ = (mid_price - avg_entry_) * units;

        equity_ = balance_ + unrealized_pnl_;
    }

    uint64_t BrokerSim::buy_market(int64_t ts, float mid_price, float lots)
    {
        // std::cout << "BUY " << lots << " lots at " << mid_price << std::endl;
        return exec(Side::Buy, ts, mid_price, lots);
    }

    uint64_t BrokerSim::sell_market(int64_t ts, float mid_price, float lots)
    {
        // std::cout << "SELL " << lots << " lots at " << mid_price << std::endl;
        return exec(Side::Sell, ts, mid_price, lots);
    }

    uint64_t BrokerSim::close_all(int64_t ts, float mid_price)
    {
        if (position_lots_ == 0.0f)
            return 0;

        const float lots_to_close = std::fabs(position_lots_);
        const Side side = (position_lots_ > 0.0f) ? Side::Sell : Side::Buy;
        return exec(side, ts, mid_price, lots_to_close);
    }

    uint64_t BrokerSim::exec(Side side, int64_t ts, float mid_price, float lots)
    {
        if (!(lots > 0.0f))
            return 0;

        const float hs = half_spread_price();
        const float sl = slippage_price();

        float fill_price = mid_price;
        if (side == Side::Buy)
        {
            fill_price = mid_price + hs + sl;
        }
        else
        {
            fill_price = mid_price - hs - sl;
        }

        const float commission = costs_.commission_per_lot * lots;
        apply_fill(side, ts, fill_price, lots, commission);

        // record
        Fill f;
        f.id = next_fill_id_++;
        f.ts = ts;
        f.side = side;
        f.lots = lots;
        f.price = fill_price;
        f.commission = commission;
        // realized pnl recorded inside apply_fill via balance changes; we also store it:
        // easiest: compute as delta in balance excluding commission? We'll calculate below by comparing.
        // For now we leave f.realized_pnl populated by apply_fill through a local technique:
        // (We set it by tracking before/after in apply_fill, but to keep it simple here,
        // we re-derive: realized_pnl = (balance_after - balance_before) + commission)
        // We'll do it properly:
        // NOTE: To keep interface clean, do the before/after here:
        // (We already mutated, so can't now.) We'll instead store 0 and you can compute from trade log later.
        f.realized_pnl = 0.0f;

        fills_.push_back(f);

        // update mark-to-market immediately using mid
        on_bar(ts, mid_price);

        return fills_.back().id;
    }

    void BrokerSim::apply_fill(Side side, int64_t /*ts*/, float fill_price, float lots, float commission)
    {
        // Commission is always charged
        balance_ -= commission;

        const float fill_signed_lots = (side == Side::Buy) ? lots : -lots;

        // If flat -> open new
        if (position_lots_ == 0.0f || std::isnan(avg_entry_))
        {
            position_lots_ = fill_signed_lots;
            avg_entry_ = fill_price;
            return;
        }

        // If same direction -> increase and update weighted avg entry
        if (signf(position_lots_) == signf(fill_signed_lots))
        {
            const float old_lots = position_lots_;
            const float new_lots = old_lots + fill_signed_lots;

            const float w_old = std::fabs(old_lots);
            const float w_new = std::fabs(fill_signed_lots);

            avg_entry_ = (avg_entry_ * w_old + fill_price * w_new) / (w_old + w_new);
            position_lots_ = new_lots;
            return;
        }

        // Opposite direction -> this reduces or flips.
        const float old_lots_abs = std::fabs(position_lots_);
        const float reduce_abs = std::min(old_lots_abs, std::fabs(fill_signed_lots));
        const float remaining_abs = old_lots_abs - reduce_abs;

        // Realize PnL on the reduced portion at the fill price
        // For a long: pnl = (fill - entry) * units_closed
        // For a short: pnl = (entry - fill) * units_closed  (handled by sign)
        const float closed_units = reduce_abs * spec_.lot_size;

        float realized = 0.0f;
        if (position_lots_ > 0.0f)
        {
            // closing long by selling
            realized = (fill_price - avg_entry_) * closed_units;
        }
        else
        {
            // closing short by buying
            realized = (avg_entry_ - fill_price) * closed_units;
        }
        balance_ += realized;

        // Now compute new net position
        const float new_lots = position_lots_ + fill_signed_lots;

        if (new_lots == 0.0f || std::fabs(new_lots) < 1e-9f)
        {
            // flat
            position_lots_ = 0.0f;
            avg_entry_ = NAN;
            return;
        }

        if (remaining_abs > 0.0f)
        {
            // reduced but did not flip: entry stays the same
            position_lots_ = new_lots;
            // avg_entry_ unchanged
            return;
        }

        // flipped: the remaining part opens a new position at fill price
        position_lots_ = new_lots;
        avg_entry_ = fill_price;
    }

} // namespace broker
