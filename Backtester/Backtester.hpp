#pragma once

#include "DataHandler/TapeReader.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Forward declare Chart to avoid including headers
namespace chart {
    class Chart;
}

namespace backtest {

// Main backtest controller. Initialise with symbol, start/end dates, timeframe.
// Run with onStart/onBar callbacks. Use getBar/getBarRange and now() inside callbacks.
class Backtester {
public:
    Backtester(std::string base_dir,
              std::string symbol,
              int start_ymd,
              int end_ymd,
              std::string timeframe);

    // Run the backtest: calls onStart once, then loops over bars calling onBar.
    // If chart is enabled, updates and renders the chart each iteration.
    void run(std::function<void(Backtester&)> onStart,
             std::function<void(Backtester&)> onBar);

    // Initialize chart visualization (optional). Call before run().
    // Returns true if chart initialized successfully.
    bool chartInitialise(const std::string& title = "Backtest Chart",
                        int width = 1280,
                        int height = 720,
                        size_t max_points = 5000,
                        int bars_per_frame = 1);

    // Current bar timestamp (nanoseconds). Updates each iteration.
    uint64_t now() const { return now_; }

    // Return timestamp for n bars before current (e.g. nowMinusBars(10) for "now - 10 bars").
    uint64_t nowMinusBars(int n) const;

    // Get single bar at datetime (from history). Returns nullopt if not found.
    std::optional<datahandler::Bar1m> getBar(const std::string& timeframe,
                                             uint64_t datetime_ns) const;

    // Get bars in range [start_ns, end_ns] inclusive (from history).
    std::vector<datahandler::Bar1m> getBarRange(const std::string& timeframe,
                                                uint64_t start_ns,
                                                uint64_t end_ns) const;

    // Current bar being processed.
    const datahandler::Bar1m& currentBar() const { return current_bar_; }

    // Metadata accessors.
    const std::string& symbol() const { return reader_.symbol(); }
    const std::string& timeframe() const { return reader_.timeframe(); }
    int start_date() const { return reader_.start_date(); }
    int end_date() const { return reader_.end_date(); }

private:
    bool advance();
    uint64_t barSizeNs() const;

    datahandler::TapeReader reader_;
    datahandler::Bar1m current_bar_;
    uint64_t now_;
    std::vector<datahandler::Bar1m> history_;
    bool has_current_ = false;
    chart::Chart* chart_ = nullptr;  // Optional chart visualization
};

}  // namespace backtest
