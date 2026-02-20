#include "core/EngineCtxBridge.h"
#include <cmath>
#include <iostream>

static inline EngineUserState *U(EngineCtx *ctx)
{
    return reinterpret_cast<EngineUserState *>(ctx->user);
}

static FeatureRef ctx_get_feature(EngineCtx *ctx, int feature_type, int period)
{
    auto *u = U(ctx);

    if (feature_type == FEAT_EMA)
    {
        auto it = u->ema_cache.find(period);
        if (it == u->ema_cache.end())
            return {nullptr, 0};
        return {it->second.data(), it->second.size()};
    }
    if (feature_type == FEAT_ATR)
    {
        auto it = u->atr_cache.find(period);
        if (it == u->atr_cache.end())
            return {nullptr, 0};
        return {it->second.data(), it->second.size()};
    }

    return {nullptr, 0};
}

static uint64_t ctx_buy_market(EngineCtx *ctx, float lots, float sl, float tp)
{
    (void)sl;
    (void)tp; // v1: ignore brackets; add later in BrokerSim
    auto *u = U(ctx);
    return u->broker->buy_market(ctx->bar.ts, ctx->bar.close, lots);
}

static uint64_t ctx_sell_market(EngineCtx *ctx, float lots, float sl, float tp)
{
    (void)sl;
    (void)tp;
    auto *u = U(ctx);
    return u->broker->sell_market(ctx->bar.ts, ctx->bar.close, lots);
}

static uint64_t ctx_close_all(EngineCtx *ctx)
{
    auto *u = U(ctx);
    return u->broker->close_all(ctx->bar.ts, ctx->bar.close);
}

static float ctx_equity(EngineCtx *ctx)
{
    return U(ctx)->broker->equity();
}
static float ctx_balance(EngineCtx *ctx)
{
    return U(ctx)->broker->balance();
}
static float ctx_position_lots(EngineCtx *ctx)
{
    return U(ctx)->broker->position_lots();
}
static float ctx_avg_entry(EngineCtx *ctx)
{
    return U(ctx)->broker->avg_entry();
}

// Call this once to wire function pointers
void init_engine_ctx(EngineCtx &ctx, EngineUserState &user)
{
    ctx.get_feature = &ctx_get_feature;

    ctx.buy_market = &ctx_buy_market;
    ctx.sell_market = &ctx_sell_market;
    ctx.close_all = &ctx_close_all;

    ctx.equity = &ctx_equity;
    ctx.balance = &ctx_balance;
    ctx.position_lots = &ctx_position_lots;
    ctx.avg_entry = &ctx_avg_entry;

    ctx.user = &user;
}
