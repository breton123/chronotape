cmake_minimum_required(VERSION 3.16)
project(BacktestingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build GLFW from vendored source ---
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)


add_executable(backtest
    main.cpp
    src/data/MMapFile.cpp
    src/data/DateUtils.cpp
    src/data/TapeReader.cpp
    src/features/FeatureManager.cpp
    src/strategy/PluginLoader.cpp
    src/core/BacktestRunner.cpp
    src/broker/BrokerSim.cpp
    src/results/MetricsEngine.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl2.cpp
    external/implot/implot.cpp
    external/implot/implot_items.cpp
)

# ---- Strategy DLL: EmaFlipStrategy ----
add_library(EmaFlipStrategy SHARED
    src/strategy/EmaFlipStrategy.cpp
)

# Make sure the strategy can include your shared headers
target_include_directories(EmaFlipStrategy PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# If your headers rely on C++17 etc, match your main target
target_compile_features(EmaFlipStrategy PRIVATE cxx_std_17)

# Optional: put DLL next to your exe so PluginLoader can find it easily
set_target_properties(EmaFlipStrategy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(backtest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    external/imgui
    external/imgui/backends
    external/implot
    external/glfw/include
)

target_link_libraries(backtest PRIVATE PRIVATE glfw opengl32)

target_sources(backtest PRIVATE
    src/strategy/PluginLoader.cpp
    src/core/EngineCTxBridge.cpp
    src/features/RunPackWriter.cpp
)

# Windows-only (mmap uses Windows API)
if(WIN32)
    # No extra libs needed for CreateFile/MapViewOfFile

    # ImGui/ImPlot/GLFW libraries (adjust paths as needed)
    # Example: target_link_libraries(backtest imgui implot glfw3 opengl32)
    # Or use find_package() if libraries are installed via vcpkg/conan/etc
endif()
