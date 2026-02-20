#pragma once
#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#define STRAT_API __declspec(dllexport)
#else
#define STRAT_API
#endif

// Keep this header C-friendly.
extern "C"
{

    // Minimal bar view (current bar only).
    struct BarView
    {
        int64_t ts;
        float open, high, low, close, volume;
        size_t index; // bar index in this run
    };

    // Feature array view: strategy reads data[i]
    struct FeatureRef
    {
        const float *data;
        size_t len;
    };

    // Feature type enum (stable across DLL boundary)
    enum FeatureType : int
    {
        FEAT_EMA = 1,
        FEAT_ATR = 2,
        // later: RSI, SMA, STD, ZSCORE, HH, LL...
    };

    struct EngineCtx;

    // Function table: strategy calls these, engine implements them.
    typedef FeatureRef (*FnGetFeature)(EngineCtx *ctx, int feature_type, int period);

    typedef uint64_t (*FnBuyMarket)(EngineCtx *ctx, float lots, float sl, float tp);
    typedef uint64_t (*FnSellMarket)(EngineCtx *ctx, float lots, float sl, float tp);
    typedef uint64_t (*FnCloseAll)(EngineCtx *ctx);

    typedef float (*FnEquity)(EngineCtx *ctx);
    typedef float (*FnBalance)(EngineCtx *ctx);
    typedef float (*FnPositionLots)(EngineCtx *ctx);
    typedef float (*FnAvgEntry)(EngineCtx *ctx);

    // Opaque context passed into strategies
    struct EngineCtx
    {
        // Current bar
        BarView bar;

        // Function table
        FnGetFeature get_feature;

        FnBuyMarket buy_market;
        FnSellMarket sell_market;
        FnCloseAll close_all;

        FnEquity equity;
        FnBalance balance;
        FnPositionLots position_lots;
        FnAvgEntry avg_entry;

        // Engine-owned pointer (strategy MUST NOT touch)
        void *user;
    };

} // extern "C"
