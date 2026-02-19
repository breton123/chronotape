#include "Backtester/Backtester.hpp"
#include "Chart/Chart.hpp"
#include <algorithm>
#include <stdexcept>

namespace backtest {

namespace {
    uint64_t barSizeFromTimeframe(const std::string& tf) {
        if (tf == "1m") return 60ULL * 1'000'000'000;   // 60 seconds in ns
        if (tf == "5m") return 5ULL * 60 * 1'000'000'000;
        if (tf == "15m") return 15ULL * 60 * 1'000'000'000;
        if (tf == "1h") return 60ULL * 60 * 1'000'000'000;
        if (tf == "4h") return 4ULL * 60 * 60 * 1'000'000'000;
        if (tf == "1d") return 24ULL * 60 * 60 * 1'000'000'000;
        throw std::runtime_error("Unsupported timeframe: " + tf);
    }
}

Backtester::Backtester(std::string base_dir,
                       std::string symbol,
                       int start_ymd,
                       int end_ymd,
                       std::string timeframe)
    : reader_(std::move(base_dir), std::move(symbol), std::move(timeframe),
              start_ymd, end_ymd)
    , now_(0)
    , has_current_(false)
    , chart_(nullptr)
{
}

void Backtester::run(std::function<void(Backtester&)> onStart,
                     std::function<void(Backtester&)> onBar)
{
    onStart(*this);

    while (advance()) {
        // Add bar to chart if enabled
        if (chart_) {
            chart_->addBar(current_bar_);
        }

        onBar(*this);

        // Update and render chart if enabled
        if (chart_) {
            if (!chart_->updateAndRender()) {
                // Window closed, continue without chart
                delete chart_;
                chart_ = nullptr;
            }
        }
    }

    // Cleanup chart
    if (chart_) {
        delete chart_;
        chart_ = nullptr;
    }
}

bool Backtester::advance() {
    has_current_ = reader_.nextBar(current_bar_);
    if (has_current_) {
        now_ = current_bar_.ts_ns;
        history_.push_back(current_bar_);
    }
    return has_current_;
}

uint64_t Backtester::barSizeNs() const {
    return barSizeFromTimeframe(reader_.timeframe());
}

uint64_t Backtester::nowMinusBars(int n) const {
    if (n <= 0) return now_;
    return now_ - static_cast<uint64_t>(n) * barSizeNs();
}

std::optional<datahandler::Bar1m> Backtester::getBar(const std::string& timeframe,
                                                     uint64_t datetime_ns) const
{
    (void)timeframe;
    uint64_t barSize = barSizeNs();

    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (it->ts_ns <= datetime_ns && datetime_ns < it->ts_ns + barSize) {
            return *it;
        }
        if (it->ts_ns < datetime_ns) break;
    }
    return std::nullopt;
}

std::vector<datahandler::Bar1m> Backtester::getBarRange(const std::string& timeframe,
                                                        uint64_t start_ns,
                                                        uint64_t end_ns) const
{
    (void)timeframe;
    std::vector<datahandler::Bar1m> result;

    for (const auto& bar : history_) {
        if (bar.ts_ns >= start_ns && bar.ts_ns <= end_ns) {
            result.push_back(bar);
        }
        if (bar.ts_ns > end_ns) break;
    }
    return result;
}

bool Backtester::chartInitialise(const std::string& title,
                                 int width,
                                 int height,
                                 size_t max_points,
                                 int bars_per_frame)
{
    if (chart_) {
        return true;  // Already initialized
    }

    chart_ = chart::chartInitialise(title, width, height, max_points, bars_per_frame);
    return chart_ != nullptr;
}

}  // namespace backtest
