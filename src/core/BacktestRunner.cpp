#include "data/TapeReader.hpp"
#include "features/FeatureManager.h"
#include "broker/BrokerSim.h"
#include "core/EngineCtxBridge.h"
#include "strategy/PluginLoader.h"
#include "results/RunRecorder.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <windows.h>
#include <iostream>

using namespace datahandler;
using namespace features;

#define PRINT_LAST(name, vec, fmt) \
    std::printf("%-30s : " fmt "\n", name, (vec).back())

class BacktestRunner
{
public:
    static void on_closed_trade_cb(void *user, const broker::ClosedTrade &ct)
    {
        auto *rec = reinterpret_cast<RunRecorder *>(user);

        ClosedTrade t{};
        t.entry_ts = ct.entry_ts;
        t.exit_ts = ct.exit_ts;
        t.entry_i = ct.entry_i;
        t.exit_i = ct.exit_i;
        t.lots = ct.lots_closed;
        t.entry_price = ct.entry_price;
        t.exit_price = ct.exit_price;
        t.pnl = ct.realized_pnl;
        t.side = (ct.side == broker::Side::Buy) ? TradeSide::Long : TradeSide::Short;
        // t.hold_bars = t.exit_i - t.entry_i;

        rec->on_trade_closed(t);
    }

    void run()
    {
        std::setvbuf(stdout, nullptr, _IONBF, 0);

        try
        {
            // Connect to data bar stream
            const std::string base_dir = "C:/Users/Louis/Desktop/CPP Fast Backtesting Engine/data/tapes";
            const std::string symbol = "EURUSD";
            const std::string timeframe = "1m";
            const int start_ymd = 20230101;
            const int end_ymd = 20251231;
            TapeReader reader(base_dir, symbol, timeframe, start_ymd, end_ymd);

            std::printf("Starting Backtest: %s %s %d-%d\n", symbol.c_str(), timeframe.c_str(), start_ymd, end_ymd);
            std::printf("Base dir: %s\n", base_dir.c_str());

            // Create broker
            broker::SymbolSpec spec{0.0001f, 100000.0f};
            broker::CostsModel costs{0.8f, 0.1f, 0.0f};
            broker::BrokerSim br(spec, costs, 100000.0f);

            // Create Metrics Tracker
            RunRecorder rec({100000.0f, 252 * 24 * 60});
            LONG64 expected_bars = (end_ymd - start_ymd + 1) * 24 * 60; // lol stack overflow incoming
            rec.reserve(expected_bars, 5000);

            br.set_on_closed_trade(&on_closed_trade_cb, &rec);

            // Create feature manager
            FeatureManager fm;

            // Initialize Engine
            EngineUserState user;
            user.feats = &fm;
            user.broker = &br;

            EngineCtx ctx{};
            init_engine_ctx(ctx, user);

            strategy::PluginLoader plugin;
            plugin.load("C:/Users/Louis/Desktop/CPP Fast Backtesting Engine/build/Release/EmaFlipStrategy.dll"); // or .so
            plugin.create(R"({"risk":0.1,"ema":50})");

            std::printf("Loaded Strategy: EmaFlipStrategy\n");

            plugin.on_start(&ctx);

            // Create Indicators
            auto ema50 = fm.require_ema(50);
            auto atr14 = fm.require_atr(14);

            // ensure caches exist
            user.ema_cache[50] = {};
            user.atr_cache[14] = {};

            size_t i = 0;
            Bar1m bar{};
            while (reader.nextBar(bar))
            {
                if (i == 0)
                    std::cout << "CALLING on_bar at i=0\n";
                // update ctx bar
                ctx.bar.ts = bar.ts_ns;
                ctx.bar.open = bar.open;
                ctx.bar.high = bar.high;
                ctx.bar.low = bar.low;
                ctx.bar.close = bar.close;
                ctx.bar.volume = bar.volume;
                ctx.bar.index = i;

                // update features & broker
                fm.update(bar.open, bar.high, bar.low, bar.close, bar.volume);
                br.on_bar(bar.ts_ns, bar.close);

                const bool in_market = (br.position_lots() != 0.0f);
                rec.on_bar(bar.ts_ns, br.balance(), br.equity(), br.unrealized_pnl(), in_market);
                br.set_bar_index((int)i);

                // append feature values to arrays (NaN until ready)
                user.ema_cache[50].push_back(ema50.stream->ready ? ema50.stream->value : NAN);
                user.atr_cache[14].push_back(atr14.stream->ready ? atr14.stream->value : NAN);

                auto fr = ctx.get_feature(&ctx, FEAT_EMA, 50);
                if (i == 0)
                {
                    std::cout << "EMA FeatureRef: data=" << (void *)fr.data << " len=" << fr.len << "\n";
                }
                if (i == 200)
                {
                    std::cout << "EMA[200]=" << fr.data[200] << "\n";
                }

                // call strategy
                plugin.on_bar(&ctx);

                ++i;
            }

            plugin.on_end(&ctx);
            auto m = br.metrics();
            std::printf("Metrics: bars=%d, balance=%.2f, equity=%.2f, max_equity=%.2f, net_profit=%.2f, total_trades=%d, max_balance=%.2f, max_drawdown=%.2f\n", m.bars, m.balance, m.equity, m.max_equity, m.net_profit, m.total_trades, m.max_balance, m.max_balance_dd);

            const auto &r = rec.series();

            std::printf("Recorded %zu bars\n\n", r.ts.size());

            PRINT_LAST("Balance", r.balance, "%.2f");
            PRINT_LAST("Equity", r.equity, "%.2f");
            PRINT_LAST("Net Profit", r.net_profit, "%.2f");

            PRINT_LAST("Max Equity DD", r.max_equity_dd, "%.2f");
            PRINT_LAST("Max Balance DD", r.max_balance_dd, "%.2f");
            PRINT_LAST("Pct in Equity DD", r.pct_in_equity_drawdown, "%.2f");

            PRINT_LAST("Trades", r.total_trades, "%d");
            PRINT_LAST("Winning Trades", r.winning_trades, "%d");
            PRINT_LAST("Losing Trades", r.losing_trades, "%d");
            PRINT_LAST("Win Rate", r.win_rate, "%.4f");

            PRINT_LAST("Profit Factor", r.profit_factor, "%.2f");
            PRINT_LAST("Expectancy", r.expected_value, "%.4f");

            PRINT_LAST("Sharpe", r.sharpe_ratio, "%.2f");
            PRINT_LAST("Calmar", r.calmar_ratio, "%.2f");
            PRINT_LAST("Sortino", r.sortino_ratio, "%.2f");

            std::printf("Backtest Completed. Final Equity: %.2f\n", br.equity());
        }
        catch (const std::exception &e)
        {
            std::fprintf(stderr, "Error: %s\n", e.what());
        }
    }
};