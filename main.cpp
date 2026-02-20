// Main entry point for the backtesting engine.
// Uses Backtester with OnStart and OnBar callbacks.

#include "src/core/BacktestRunner.cpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <windows.h>

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    try
    {
        BacktestRunner runner;
        runner.run();
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
