#include "data/TapeReader.hpp"
#include "features/FeatureManager.h"
#include "broker/BrokerSim.h"
#include "core/EngineCtxBridge.h"
#include "strategy/PluginLoader.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <windows.h>
#include <iostream>

using namespace datahandler;
using namespace features;

class BacktestRunner
{
public:
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

            std::printf("Backtest Completed. Final Equity: %.2f\n", br.equity());
        }
        catch (const std::exception &e)
        {
            std::fprintf(stderr, "Error: %s\n", e.what());
        }
    }
};