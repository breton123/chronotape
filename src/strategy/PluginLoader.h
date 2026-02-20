// strategy/PluginLoader.h
#pragma once
#include <string>
#include <stdexcept>
#include <utility>
#include "strategy_api.h"

namespace strategy
{

    class PluginLoader
    {
    public:
        PluginLoader() = default;
        ~PluginLoader() { unload(); }

        PluginLoader(const PluginLoader &) = delete;
        PluginLoader &operator=(const PluginLoader &) = delete;

        PluginLoader(PluginLoader &&other) noexcept { move_from(std::move(other)); }
        PluginLoader &operator=(PluginLoader &&other) noexcept
        {
            if (this != &other)
            {
                unload();
                move_from(std::move(other));
            }
            return *this;
        }

        // Load DLL/.so and resolve symbols
        void load(const std::string &path);

        // Unload library, destroy instance if needed
        void unload();

        bool loaded() const { return lib_ != nullptr; }

        // Strategy lifetime
        void create(const std::string &params_json);
        void destroy();

        // Calls
        void on_start(EngineCtx *ctx);
        void on_bar(EngineCtx *ctx);
        void on_end(EngineCtx *ctx);

    private:
        void *lib_ = nullptr; // HMODULE on Windows, void* on POSIX
        StrategyHandle handle_ = nullptr;

        FnCreate create_ = nullptr;
        FnDestroy destroy_ = nullptr;
        FnOnStart on_start_ = nullptr;
        FnOnBar on_bar_ = nullptr;
        FnOnEnd on_end_ = nullptr;

        void move_from(PluginLoader &&other) noexcept
        {
            lib_ = other.lib_;
            other.lib_ = nullptr;
            handle_ = other.handle_;
            other.handle_ = nullptr;
            create_ = other.create_;
            other.create_ = nullptr;
            destroy_ = other.destroy_;
            other.destroy_ = nullptr;
            on_start_ = other.on_start_;
            other.on_start_ = nullptr;
            on_bar_ = other.on_bar_;
            other.on_bar_ = nullptr;
            on_end_ = other.on_end_;
            other.on_end_ = nullptr;
        }

        [[noreturn]] void fail(const std::string &msg) const
        {
            throw std::runtime_error("PluginLoader: " + msg);
        }
    };

} // namespace strategy
