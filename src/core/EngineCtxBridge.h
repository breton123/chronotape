#pragma once
#include "core/EngineCtx.h"
#include "features/FeatureManager.h"
#include "broker/BrokerSim.h"

#include <vector>
#include <unordered_map>

struct EngineUserState
{
    features::FeatureManager *feats = nullptr;
    broker::BrokerSim *broker = nullptr;

    std::unordered_map<int, std::vector<float>> ema_cache;
    std::unordered_map<int, std::vector<float>> atr_cache;
};

void init_engine_ctx(EngineCtx &ctx, EngineUserState &user);
