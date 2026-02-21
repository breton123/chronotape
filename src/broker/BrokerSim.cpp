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
        bars_++;

        // IF not in a position. Return
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

        // Balance Drawdowns
        if (balance_ > max_balance_)
        {
            max_balance_ = balance_;
        }
        dd_balance_ = balance_ - max_balance_;
        if (dd_balance_ > 0.0f)
            dd_balance_ = 0.0f;

        max_balance_dd_ = std::min(max_balance_dd_, balance_ - max_balance_);
        if (max_balance_dd_ > 0.0f)
        {
            max_balance_dd_ = 0.0f;
        }
        else
        {
            bars_in_balance_drawdown_++;
            pct_in_balance_drawdown_ = (bars_in_balance_drawdown_ / static_cast<float>(bars_)) * 100;
        }

        // Equity Drawdowns
        if (equity_ > max_equity_)
        {
            max_equity_ = equity_;
        }
        equity_ = balance_ + unrealized_pnl_;
        dd_equity_ = equity_ - max_equity_;
        if (dd_equity_ > 0.0f)
            dd_equity_ = 0.0f;

        max_equity_dd_ = std::min(max_equity_dd_, equity_ - max_equity_);
        if (max_equity_dd_ > 0.0f)
        {
            max_equity_dd_ = 0.0f;
        }
        else
        {
            bars_in_equity_drawdown_++;
            pct_in_equity_drawdown_ = (bars_in_equity_drawdown_ / static_cast<float>(bars_)) * 100;
        }

        if (equity_ <= 0.0f)
        {
            // Account is blown. Reset everything to avoid weird states.
            balance_ = 0.0f;
            equity_ = 0.0f;
            position_lots_ = 0.0f;
            avg_entry_ = NAN;

            entry_ts_ = 0;
            entry_i_ = -1;

            // Note: we keep drawdown and metrics state to reflect the blowup.
        }
        //
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

        if (balance_ <= 0.0f)
        {
            // std::cerr << "Account is blown. No further trades allowed.\n";
            account_blown_ = true;
            return 0;
        }

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

        apply_fill(side, ts, fill_price, lots, commission);

        fills_.push_back(f);

        // update mark-to-market immediately using mid
        on_bar(ts, mid_price);

        return fills_.back().id;
    }

    void BrokerSim::apply_fill(Side side, int64_t ts, float fill_price, float lots, float commission)
    {
        // Commission is always charged
        balance_ -= commission;
        lots = lots / 1000;  //LOWERING JUST FOR TESTING

        const float fill_signed_lots = (side == Side::Buy) ? lots : -lots;

        // If flat -> open new
        if (position_lots_ == 0.0f || std::isnan(avg_entry_))
        {
            position_lots_ = fill_signed_lots;
            avg_entry_ = fill_price;

            entry_ts_ = ts; // <-- change signature to pass ts into apply_fill
            entry_i_ = bar_index_;
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

        if (reduce_abs > 0.0f && on_closed_trade_)
        {
            ClosedTrade ct;
            ct.entry_ts = entry_ts_;
            ct.exit_ts = ts;
            ct.entry_i = entry_i_;
            ct.exit_i = bar_index_;

            ct.entry_price = avg_entry_;
            ct.exit_price = fill_price;

            ct.lots_closed = reduce_abs;
            ct.side = (position_lots_ > 0.0f) ? Side::Buy : Side::Sell; // original direction

            ct.realized_pnl = realized;
            ct.commission = commission; // optional; this is closing fill commission, may include open/close later

            on_closed_trade_(on_closed_trade_user_, ct);
        }

        // Now compute new net position
        const float new_lots = position_lots_ + fill_signed_lots;

        if (new_lots == 0.0f || std::fabs(new_lots) < 1e-9f)
        {
            // flat
            position_lots_ = 0.0f;
            avg_entry_ = NAN;
            entry_ts_ = 0;
            entry_i_ = -1;

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

        entry_ts_ = ts;
        entry_i_ = bar_index_;
    }

} // namespace broker
