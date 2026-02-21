#include "data/TapeReader.hpp"
#include "features/FeatureManager.h"
#include "broker/BrokerSim.h"
#include "core/EngineCtxBridge.h"
#include "strategy/PluginLoader.h"
#include "results/RunRecorder.h"
#include "features/RunPackWriter.h"
#include "data/DateUtils.hpp"
#include "data/TapeTypes.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <cstring>

using namespace datahandler;
using namespace features;

#define PRINT_LAST(name, vec, fmt) \
    std::printf("%-30s : " fmt "\n", name, (vec).back())

static bool compute_time_range(const std::string &base_dir,
                               const std::string &symbol,
                               const std::string &timeframe,
                               int start_ymd,
                               int end_ymd,
                               uint64_t &out_start_ts,
                               uint64_t &out_end_ts)
{
    bool have_start = false;
    uint64_t first_ts = 0;
    uint64_t last_ts = 0;

    int day = start_ymd;
    while (day <= end_ymd)
    {
        std::string path = make_tape_path(base_dir, symbol, timeframe, day);
        day = next_day(day);

        if (!file_exists(path))
            continue;

        std::ifstream f(path, std::ios::binary);
        if (!f)
            continue;

        TapeHeader hdr{};
        f.read(reinterpret_cast<char *>(&hdr), sizeof(TapeHeader));
        if (!f)
            continue;

        if (std::memcmp(hdr.magic, "TAPEv001", 8) != 0 ||
            hdr.version != 1 ||
            hdr.record_type != 2 ||
            hdr.record_size != sizeof(Bar1m))
        {
            continue;
        }

        if (!have_start)
        {
            have_start = true;
            first_ts = hdr.start_ts_ns;
        }
        last_ts = hdr.end_ts_ns;
    }

    if (!have_start)
        return false;

    out_start_ts = first_ts;
    out_end_ts = last_ts;
    return true;
}

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
            const std::string base_dir = "C:/Users/louis/Desktop/Project/chronotape/data/tapes";
            const std::string symbol = "EURUSD";
            const std::string timeframe = "1m";
            const int start_ymd = 20000101;
            const int end_ymd = 20251231;
            TapeReader reader(base_dir, symbol, timeframe, start_ymd, end_ymd);

            uint64_t data_start_ts = 0;
            uint64_t data_end_ts = 0;
            bool has_time_range = compute_time_range(base_dir, symbol, timeframe, start_ymd, end_ymd, data_start_ts, data_end_ts);
            uint64_t ts_span = (has_time_range && data_end_ts > data_start_ts) ? (data_end_ts - data_start_ts) : 0;
            int last_progress_pct = 0;

            std::printf("Starting Backtest: %s %s %d-%d\n", symbol.c_str(), timeframe.c_str(), start_ymd, end_ymd);
            std::printf("Base dir: %s\n", base_dir.c_str());

            // Create broker
            broker::SymbolSpec spec{0.0001f, 100000.0f};
            broker::CostsModel costs{0.8f, 0.1f, 0.0f};
            broker::BrokerSim br(spec, costs, 100000.0f);

            // Create Metrics Tracker
            RunRecorder rec({100000.0f, 252 * 24 * 60});
            size_t expected_bars = (end_ymd - start_ymd + 1) * 24 * 60; // lol stack overflow incoming
            const size_t HARD_CAP = 50000000; // pick a sane ceiling for your machine
            expected_bars = std::min(expected_bars, HARD_CAP);
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
            plugin.load("C:/Users/louis/Desktop/Project/chronotape/build/libEmaFlipStrategy.dll"); // or .so
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
                if (ts_span > 0 && bar.ts_ns >= data_start_ts)
                {
                    uint64_t num = bar.ts_ns - data_start_ts;
                    double progress = static_cast<double>(num) / static_cast<double>(ts_span);
                    if (progress < 0.0)
                        progress = 0.0;
                    if (progress > 1.0)
                        progress = 1.0;
                    int pct = static_cast<int>(progress * 100.0);
                    if (pct >= last_progress_pct + 1 && pct <= 100)
                    {
                        last_progress_pct = pct;
                        std::printf("Progress: %3d%% (ts=%llu)\n", pct, static_cast<unsigned long long>(bar.ts_ns));
                    }
                }

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

                if (br.account_blown())
                {
                    std::cout << "Account blown at bar " << i << std::endl;
                    break;
                }

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

            rec.finalize();
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

            // after backtest
            RunPackWriter w;
            RunPackWriter::Meta meta;
            // meta.created_unix_ms = /* now in ms */;
            meta.meta_json = R"({"symbol":"EURUSD","tf":"M1","strategy":"EmaFlipStrategy","params":{"ema_period":50,"lots":0.1}})";

            std::string err;
            bool ok = w.write("runs/run_000001.rpack", meta, rec.series(), rec.trades(), &err);
            if (!ok)
            {
                std::printf("RunPack write failed: %s\n", err.c_str());
            }
            else
            {
                std::printf("Saved runpack.\n");
            }
        }
        catch (const std::exception &e)
        {
            std::fprintf(stderr, "Error: %s\n", e.what());
        }
    }
};