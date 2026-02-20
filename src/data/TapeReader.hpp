#pragma once

#include "TapeTypes.hpp"
#include "MMapFile.hpp"
#include <string>

namespace datahandler
{

    struct BarContext
    {
        size_t index;
        int64_t ts;
        float open, high, low, close, volume;
    };

    // Streams 1m bars from tape files across a date range.
    // Use nextBar() in a loop from your backtest engine.
    class TapeReader
    {
    public:
        TapeReader(std::string base_dir,
                   std::string symbol,
                   std::string timeframe,
                   int start_ymd,
                   int end_ymd);

        // Returns true if a bar was filled, false when no more bars.
        bool nextBar(Bar1m &out);

        // Accessors for metadata.
        const std::string &symbol() const { return symbol_; }
        const std::string &timeframe() const { return timeframe_; }
        int start_date() const { return start_ymd_; }
        int end_date() const { return end_ymd_; }

    private:
        bool open_next_tape();

        std::string base_dir_;
        std::string symbol_;
        std::string timeframe_;
        int start_ymd_;
        int end_ymd_;

        int current_day_;
        uint64_t bar_index_;
        uint64_t bar_count_;
        const Bar1m *recs_;
        MMapFile mmap_;
    };

} // namespace datahandler
