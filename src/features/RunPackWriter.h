#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <type_traits>
#include <cstring>

#include "RunPackFormat.h"
#include "results/RunSeries.h"
#include "results/TradeLog.h"   // your TradeLog that stores ClosedTrade
#include "results/TradeTypes.h" // your ClosedTrade/TradeSide

class RunPackWriter
{
public:
    struct Meta
    {
        // Keep this as a single string for v1 (JSON recommended).
        // Example: {"symbol":"EURUSD","tf":"M1","start":20200101,"end":20240101,"strategy":"EmaFlip","params":{...}}
        std::string meta_json;
        int64_t created_unix_ms = 0;
    };

    // Write a complete runpack in one shot.
    // - series: your per-bar metrics (SoA)
    // - trades: closed trades
    // Returns true on success.
    bool write(const std::string &path,
               const Meta &meta,
               const RunSeries &series,
               const TradeLog &trades,
               std::string *err = nullptr);

private:
    struct SeriesDesc
    {
        char name[32];
        runpack::DType dtype;
        uint32_t elem_size;
        uint64_t len;
        const void *data;
    };

    // Collect all vectors in RunSeries into a list of SeriesDesc
    static std::vector<SeriesDesc> build_series_desc(const RunSeries &s);

    // Helpers
    static void set_name32(char out[32], const char *name);
    static uint64_t tellp_u64(std::ofstream &f);
    static void write_u64(std::ofstream &f, uint64_t v);

    template <typename T>
    static runpack::DType dtype_of();

    template <typename T>
    static SeriesDesc make_desc(const char *name, const std::vector<T> &v);

    static std::vector<runpack::TradeDiskV1> convert_trades(const TradeLog &trades);
};