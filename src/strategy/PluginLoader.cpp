// strategy/PluginLoader.cpp
#include "PluginLoader.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace strategy
{

    static std::string last_dl_error()
    {
#ifdef _WIN32
        DWORD err = GetLastError();
        if (err == 0)
            return {};
        LPSTR buf = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buf, 0, NULL);
        std::string msg = (size && buf) ? std::string(buf, size) : std::string("Unknown Win32 error");
        if (buf)
            LocalFree(buf);
        return msg;
#else
        const char *e = dlerror();
        return e ? std::string(e) : std::string();
#endif
    }

    template <typename T>
    static T load_symbol(void *lib, const char *name)
    {
#ifdef _WIN32
        auto sym = GetProcAddress((HMODULE)lib, name);
        return reinterpret_cast<T>(sym);
#else
        auto sym = dlsym(lib, name);
        return reinterpret_cast<T>(sym);
#endif
    }

    void PluginLoader::load(const std::string &path)
    {
        unload(); // in case

#ifdef _WIN32
        HMODULE h = LoadLibraryA(path.c_str());
        if (!h)
            fail("LoadLibrary failed for '" + path + "': " + last_dl_error());
        lib_ = (void *)h;
#else
        void *h = dlopen(path.c_str(), RTLD_NOW);
        if (!h)
            fail("dlopen failed for '" + path + "': " + last_dl_error());
        lib_ = h;
#endif

        // Resolve required exports
        create_ = load_symbol<FnCreate>(lib_, "strategy_create");
        destroy_ = load_symbol<FnDestroy>(lib_, "strategy_destroy");
        on_start_ = load_symbol<FnOnStart>(lib_, "strategy_on_start");
        on_bar_ = load_symbol<FnOnBar>(lib_, "strategy_on_bar");
        on_end_ = load_symbol<FnOnEnd>(lib_, "strategy_on_end");

        if (!create_)
            fail("Missing export: strategy_create");
        if (!destroy_)
            fail("Missing export: strategy_destroy");
        if (!on_start_)
            fail("Missing export: strategy_on_start");
        if (!on_bar_)
            fail("Missing export: strategy_on_bar");
        if (!on_end_)
            fail("Missing export: strategy_on_end");
    }

    void PluginLoader::unload()
    {
        // destroy instance first
        destroy();

        if (!lib_)
            return;

#ifdef _WIN32
        FreeLibrary((HMODULE)lib_);
#else
        dlclose(lib_);
#endif
        lib_ = nullptr;

        create_ = nullptr;
        destroy_ = nullptr;
        on_start_ = nullptr;
        on_bar_ = nullptr;
        on_end_ = nullptr;
    }

    void PluginLoader::create(const std::string &params_json)
    {
        if (!lib_ || !create_)
            fail("create() called but plugin not loaded");
        if (handle_)
            destroy(); // recreate allowed

        handle_ = create_(params_json.c_str());
        if (!handle_)
            fail("strategy_create returned null");
    }

    void PluginLoader::destroy()
    {
        if (handle_ && destroy_)
        {
            destroy_(handle_);
            handle_ = nullptr;
        }
    }

    void PluginLoader::on_start(EngineCtx *ctx)
    {
        if (!handle_ || !on_start_)
            fail("on_start() called before create()");
        on_start_(handle_, ctx);
    }

    void PluginLoader::on_bar(EngineCtx *ctx)
    {
        if (!handle_ || !on_bar_)
            fail("on_bar() called before create()");
        on_bar_(handle_, ctx);
    }

    void PluginLoader::on_end(EngineCtx *ctx)
    {
        if (!handle_ || !on_end_)
            fail("on_end() called before create()");
        on_end_(handle_, ctx);
    }

} // namespace strategy
