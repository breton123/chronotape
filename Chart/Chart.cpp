#include "Chart/Chart.hpp"
#include <cmath>
#include <stdexcept>

// Forward declare GLFW types
struct GLFWwindow;

// Include ImGui/ImPlot headers
// Try different include paths based on how vcpkg installs them
#if __has_include(<imgui/imgui.h>)
    // ImGui in subdirectory
    #include <imgui/imgui.h>
    #include <imgui/imgui_impl_glfw.h>
    #include <imgui/imgui_impl_opengl3.h>
#elif __has_include(<imgui.h>)
    // ImGui directly in include
    #include <imgui.h>
    #include <imgui_impl_glfw.h>
    #include <imgui_impl_opengl3.h>
#else
    // Fallback
    #include "imgui.h"
    #include "imgui_impl_glfw.h"
    #include "imgui_impl_opengl3.h"
#endif

#if __has_include(<implot/implot.h>)
    #include <implot/implot.h>
#elif __has_include(<implot.h>)
    #include <implot.h>
#else
    #include "implot.h"
#endif

#if __has_include(<GLFW/glfw3.h>)
    #include <GLFW/glfw3.h>
#elif __has_include(<glfw3.h>)
    #include <glfw3.h>
#else
    #include "glfw3.h"
#endif

namespace chart {

Chart::Chart()
    : window_(nullptr)
    , initialized_(false)
    , max_points_(5000)
    , bars_per_frame_(1)
    , bar_index_(0)
    , running_(true)
    , speed_(1.0)
{
}

Chart::~Chart() {
    if (initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        if (window_) {
            glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
        }
        glfwTerminate();
    }
}

bool Chart::initialize(const std::string& title, int width, int height) {
    if (initialized_) {
        return true;  // Already initialized
    }

    // GLFW init
    if (!glfwInit()) {
        return false;
    }

    // OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!win) {
        glfwTerminate();
        return false;
    }
    window_ = win;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);  // vsync

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    initialized_ = true;
    return true;
}

bool Chart::isOpen() const {
    if (!initialized_ || !window_) {
        return false;
    }
    GLFWwindow* win = static_cast<GLFWwindow*>(window_);
    return glfwWindowShouldClose(win) == 0;
}

void Chart::addBar(const datahandler::Bar1m& bar) {
    if (!initialized_) return;

    double x = static_cast<double>(bar_index_);
    open_series_.push(x, bar.open, max_points_);
    high_series_.push(x, bar.high, max_points_);
    low_series_.push(x, bar.low, max_points_);
    close_series_.push(x, bar.close, max_points_);
    ++bar_index_;
}

bool Chart::updateAndRender() {
    if (!initialized_ || !window_) {
        return false;
    }

    GLFWwindow* win = static_cast<GLFWwindow*>(window_);
    if (glfwWindowShouldClose(win)) {
        return false;
    }

    glfwPollEvents();

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Controls window
    ImGui::Begin("Controls");
    ImGui::Checkbox("Running", &running_);
    ImGui::SliderFloat("Speed", &speed_, 0.0f, 200.0f, "%.1f");
    int max_points_int = static_cast<int>(max_points_);
    if (ImGui::SliderInt("Max points", &max_points_int, 500, 200000)) {
        max_points_ = static_cast<size_t>(max_points_int);
    }
    ImGui::SliderInt("Bars per frame", &bars_per_frame_, 1, 100);
    ImGui::Text("Points: %zu", close_series_.x.size());
    ImGui::Text("Bar index: %llu", static_cast<unsigned long long>(bar_index_));
    ImGui::End();

    // Chart window
    ImGui::Begin("OHLC Chart");
    if (ImPlot::BeginPlot("Price", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Bar Index", "Price", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        
        if (!close_series_.x.empty()) {
            ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));  // Red for open
            ImPlot::PlotLine("Open", close_series_.x.data(), open_series_.y.data(), 
                           static_cast<int>(open_series_.x.size()));
            
            ImPlot::SetNextLineStyle(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));  // Green for high
            ImPlot::PlotLine("High", close_series_.x.data(), high_series_.y.data(), 
                           static_cast<int>(high_series_.x.size()));
            
            ImPlot::SetNextLineStyle(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));  // Blue for low
            ImPlot::PlotLine("Low", close_series_.x.data(), low_series_.y.data(), 
                           static_cast<int>(low_series_.x.size()));
            
            ImPlot::SetNextLineStyle(ImVec4(1.0f, 1.0f, 0.0f, 1.0f));  // Yellow for close
            ImPlot::PlotLine("Close", close_series_.x.data(), close_series_.y.data(), 
                           static_cast<int>(close_series_.x.size()));
        }
        ImPlot::EndPlot();
    }
    ImGui::End();

    // Render
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(win, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);

    return true;
}

Chart* chartInitialise(const std::string& title, int width, int height, 
                      size_t max_points, int bars_per_frame) {
    Chart* chart = new Chart();
    chart->setMaxPoints(max_points);
    chart->setBarsPerFrame(bars_per_frame);
    
    if (!chart->initialize(title, width, height)) {
        delete chart;
        return nullptr;
    }
    
    return chart;
}

}  // namespace chart
