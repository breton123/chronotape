#pragma once

#include "DataHandler/TapeTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chart {

// Real-time OHLC chart using ImGui + ImPlot.
// Initialize with chartInitialise(), then call update() and render() each frame.
class Chart {
public:
    Chart();
    ~Chart();

    // Initialize the chart window. Returns false on failure.
    bool initialize(const std::string& title, int width = 1280, int height = 720);

    // Add a bar to the chart (call from onBar).
    void addBar(const datahandler::Bar1m& bar);

    // Update (poll events) and render the chart. Call this in the backtest loop.
    // Returns false if window should close.
    bool updateAndRender();

    // Check if chart is initialized and window is open.
    bool isOpen() const;

    // Control chart behavior
    void setMaxPoints(size_t max_points) { max_points_ = max_points; }
    void setBarsPerFrame(int bars_per_frame) { bars_per_frame_ = bars_per_frame; }
    size_t getPointCount() const { return close_series_.x.size(); }

private:
    struct Series {
        std::vector<double> x;
        std::vector<double> y;

        void push(double xv, double yv, size_t max_points) {
            x.push_back(xv);
            y.push_back(yv);
            if (x.size() > max_points) {
                const size_t drop = x.size() - max_points;
                x.erase(x.begin(), x.begin() + drop);
                y.erase(y.begin(), y.begin() + drop);
            }
        }

        void clear() {
            x.clear();
            y.clear();
        }
    };

    void* window_;  // GLFWwindow* (void* to avoid including GLFW in header)
    bool initialized_;
    Series open_series_;
    Series high_series_;
    Series low_series_;
    Series close_series_;
    size_t max_points_;
    int bars_per_frame_;
    uint64_t bar_index_;
    bool running_;
    double speed_;
};

// Initialize chart with optional parameters. Returns pointer to Chart if successful, nullptr otherwise.
Chart* chartInitialise(const std::string& title = "Backtest Chart",
                       int width = 1280,
                       int height = 720,
                       size_t max_points = 5000,
                       int bars_per_frame = 1);

}  // namespace chart
