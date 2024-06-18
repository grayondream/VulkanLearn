#include <cstddef>
#include <stdexcept>
#include <system_error>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <format>
#include <vector>
#include <string_view>
#include <vulkan/vulkan.hpp>
#include "Application.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include "ErrorCode.hpp"
#include "VulkanInstance.hpp"
#define GLFM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
using namespace Utils;
using namespace Vulkan;

static constexpr const unsigned int WIN_HEIGHT = 600;
static constexpr const unsigned int WIN_WIDTH = 800;

std::error_code Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    pwin = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Hello Vulkan", nullptr, nullptr);
    if (!pwin) {
        LOGE("can not create a new window");
        return MakeGenerateError(AppStatus::FAIL);
    }
    return {};
}

std::error_code Application::init() {
    if (auto ret = initWindow(); ret) {
        LOGE("inintialize the vulkan instance failed");
        return ret;
    }

    instance = std::make_shared<VulkanInstance>();
    if (auto ret = instance->initialize(pwin, WIN_WIDTH, WIN_HEIGHT); ret) {
        LOGE("inintialize the vulkan instance failed");
        return ret;
    }

    return {};
}

std::error_code Application::run() {
    while (!glfwWindowShouldClose(pwin)) {
        glfwPollEvents();
        instance->draw();
    }

    return {};
}

std::error_code Application::destroy() {
    instance->destroy();
    instance = nullptr;
    glfwDestroyWindow(pwin);
    glfwTerminate();
    return {};
}