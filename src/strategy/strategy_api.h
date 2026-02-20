// strategy/strategy_api.h
#pragma once
#include "core/EngineCtx.h"

#ifdef _WIN32
#define STRAT_API __declspec(dllexport)
#else
#define STRAT_API
#endif

extern "C"
{

    typedef void *StrategyHandle;

    typedef StrategyHandle (*FnCreate)(const char *params_json);
    typedef void (*FnDestroy)(StrategyHandle);

    typedef void (*FnOnStart)(StrategyHandle, EngineCtx *);
    typedef void (*FnOnBar)(StrategyHandle, EngineCtx *);
    typedef void (*FnOnEnd)(StrategyHandle, EngineCtx *);

} // extern "C"
