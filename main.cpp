// Main entry point for the backtesting engine.
// Uses Backtester with OnStart and OnBar callbacks.

#include "Backtester/Backtester.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>

#include <windows.h>

static void onStart(backtest::Backtester &bt)
{
    (void)bt;
    std::printf("OnStart: %s %s, range %d .. %d\n",
                bt.symbol().c_str(), bt.timeframe().c_str(),
                bt.start_date(), bt.end_date());
}

static void onBar(backtest::Backtester &bt)
{
    const auto &bar = bt.currentBar();
    volatile double sink = bar.open + bar.high + bar.low + bar.close + static_cast<double>(bar.volume);
    (void)sink;

    // Example: get last 10 bars (can be used in OnBar or OnStart)
    // auto last10 = bt.getBarRange(bt.timeframe(), bt.nowMinusBars(10), bt.now());
    //(void)last10;

    // Example: get single bar at current time
    // auto current = bt.getBar(bt.timeframe(), bt.now());
    // std::printf("Current bar: %f %f %f %f %f\n", current->open, current->high, current->low, current->close, current->volume);
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const std::string base_dir = "C:/Users/Louis/Desktop/CPP Fast Backtesting Engine/tapes";

    try
    {
        const std::string symbol = "EURUSD";
        const std::string timeframe = "1m";
        const int start_ymd = 20250601;
        const int end_ymd = 20251231;

        backtest::Backtester bt(base_dir, symbol, start_ymd, end_ymd, timeframe);

        // Optional: Initialize chart visualization
        // Parameters: title, width, height, max_points (chart history), bars_per_frame
        // Uncomment to enable real-time OHLC chart:
        bt.chartInitialise("EURUSD Backtest", 1280, 720, 5000, 1);

        LARGE_INTEGER freq, t0, t1;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);

        bt.run(onStart, onBar);

        QueryPerformanceCounter(&t1);
        const double seconds = static_cast<double>(t1.QuadPart - t0.QuadPart) / freq.QuadPart;

        std::printf("Done. Time: %.6f s\n", seconds);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
