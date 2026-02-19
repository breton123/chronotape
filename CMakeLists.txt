cmake_minimum_required(VERSION 3.16)
project(BacktestingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(backtest
    main.cpp
    Backtester/Backtester.cpp
    Chart/Chart.cpp
    DataHandler/MMapFile.cpp
    DataHandler/DateUtils.cpp
    DataHandler/TapeReader.cpp
)

target_include_directories(backtest PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${imgui_INCLUDE_DIRS}
    ${implot_INCLUDE_DIRS}
    ${glfw3_INCLUDE_DIRS}
    ${OpenGL_INCLUDE_DIRS}
)

target_link_libraries(backtest PRIVATE glfw glad::glad)

# Windows-only (mmap uses Windows API)
if(WIN32)
    # No extra libs needed for CreateFile/MapViewOfFile

    # ImGui/ImPlot/GLFW libraries (adjust paths as needed)
    # Example: target_link_libraries(backtest imgui implot glfw3 opengl32)
    # Or use find_package() if libraries are installed via vcpkg/conan/etc
endif()
