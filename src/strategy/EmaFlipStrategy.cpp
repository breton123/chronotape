// strategies/EmaFlipStrategy.cpp
// Build this as a DLL and load it with your PluginLoader.
//
// Behavior:
// - Requests EMA(period) via ctx->get_feature()
// - When close crosses above EMA -> go long (close existing short first)
// - When close crosses below EMA -> go short (close existing long first)
// - Uses fixed lots from params (default 0.10)
//
// Notes:
// - Assumes your engine is populating ctx->bar.index and feature arrays for FEAT_EMA(period).
// - If EMA value is NaN (warmup), it does nothing.

#include "strategy/strategy_api.h"
#include <cstring>
#include <string>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <ostream>

static int parse_int_param(const char *json, const char *key, int defv)
{
    if (!json || !key)
        return defv;
    const char *p = std::strstr(json, key);
    if (!p)
        return defv;
    p = std::strchr(p, ':');
    if (!p)
        return defv;
    ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    return std::atoi(p);
}

static float parse_float_param(const char *json, const char *key, float defv)
{
    if (!json || !key)
        return defv;
    const char *p = std::strstr(json, key);
    if (!p)
        return defv;
    p = std::strchr(p, ':');
    if (!p)
        return defv;
    ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    return (float)std::atof(p);
}

struct StratState
{
    int ema_period = 50;
    float lots = 0.10f;

    float prev_close = NAN;
    float prev_ema = NAN;
    bool started = false;
};

extern "C"
{

    STRAT_API StrategyHandle strategy_create(const char *params_json)
    {
        auto *s = new StratState();

        // Expect params like: {"ema_period":50,"lots":0.10}
        s->ema_period = parse_int_param(params_json, "ema_period", 50);
        s->lots = parse_float_param(params_json, "lots", 0.10f);

        return (StrategyHandle)s;
    }

    STRAT_API void strategy_destroy(StrategyHandle h)
    {
        auto *s = (StratState *)h;
        delete s;
    }

    STRAT_API void strategy_on_start(StrategyHandle h, EngineCtx *ctx)
    {
        auto *s = (StratState *)h;
        (void)ctx;

        s->prev_close = NAN;
        s->prev_ema = NAN;
        s->started = true;
    }

    STRAT_API void strategy_on_bar(StrategyHandle h, EngineCtx *ctx)
    {
        auto *s = (StratState *)h;
        if (!s->started)
            return;

        const size_t i = ctx->bar.index;
        const float close = ctx->bar.close;

        FeatureRef ema = ctx->get_feature(ctx, FEAT_EMA, s->ema_period);
        if (!ema.data || i >= ema.len)
            return;

        const float ema_now = ema.data[i];
        if (std::isnan(ema_now))
            return;

        if (std::isnan(s->prev_close) || std::isnan(s->prev_ema))
        {
            s->prev_close = close;
            s->prev_ema = ema_now;
            return;
        }

        const bool prev_above = (s->prev_close > s->prev_ema);
        const bool now_above = (close > ema_now);

        if (prev_above != now_above)
        {
            const float pos = ctx->position_lots(ctx);

            if (now_above)
            {
                if (pos < 0.0f)
                    ctx->close_all(ctx);
                if (pos <= 0.0f)
                    ctx->buy_market(ctx, s->lots, 0.0f, 0.0f);
            }
            else
            {
                if (pos > 0.0f)
                    ctx->close_all(ctx);
                if (pos >= 0.0f)
                    ctx->sell_market(ctx, s->lots, 0.0f, 0.0f);
            }
        }

        s->prev_close = close;
        s->prev_ema = ema_now;
    }

    STRAT_API void strategy_on_end(StrategyHandle, EngineCtx *ctx)
    {
        // no-op
        ctx->close_all(ctx);
        std::cout << "Closing all positions" << std::endl;
    }

} // extern "C"
