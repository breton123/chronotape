#pragma once
#include <cstdint>

enum class TradeSide : int8_t
{
    Long = 1,
    Short = -1
};

struct ClosedTrade
{
    int64_t entry_ts = 0;
    int64_t exit_ts = 0;
    int32_t entry_i = -1;
    int32_t exit_i = -1;

    TradeSide side = TradeSide::Long;
    float lots = 0.0f;

    float entry_price = 0.0f;
    float exit_price = 0.0f;

    float pnl = 0.0f;   // realized pnl (account currency)
    float pnl_r = 0.0f; // optional: pnl / risk (if you track risk)
    float mae = 0.0f;   // optional
    float mfe = 0.0f;   // optional
};
