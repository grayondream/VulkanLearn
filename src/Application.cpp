#include <system_error>
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
#include "Utils.h"
#include "ErrorCode.hpp"

#define GLFM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
using namespace Utils;
using namespace Vulkan;
using namespace vk;

static constexpr const unsigned int WIN_HEIGHT = 600;
static constexpr const unsigned int WIN_WIDTH = 800;

#ifdef NDEBUG
const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif//NDEBUG

class VulkanInstance {
public:
    std::error_code initialize() {
        ApplicationInfo appInfo = { "Hello Vulkan", VK_MAKE_VERSION(1, 0, 0), "Everything but engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0 };
       
        auto vkExts = Vulkan::QueryVkExtensions();
        if (vkExts.empty()) {
            LOGE("Can not get any extensions from the local device!");
            return MakeGenerateError(AppStatus::FAIL);
        }

        for (auto i = 0; i < vkExts.size(); i++) {
            const auto e = vkExts[i];
            LOGI("The {} extension,version:{}, name:{}", i, e.specVersion, e.extensionName);
        }
        
#ifdef NDEBUG
        if (!Utils::Vulkan::CheckValidationLayerSupport(kValidationLayers)) {
            LOGE("The device donot support validation layer provided!");
            return MakeGenerateError(AppStatus::FAIL);
        }
#endif//NDEBUG

        vk::InstanceCreateInfo createInfo = { vk::InstanceCreateFlags{}, &appInfo };

        createInfo.enabledLayerCount = 0;
#ifdef NDEBUG
        auto glfwExts = Utils::Vulkan::QueryGlfwExtension();
        createInfo.enabledLayerCount = glfwExts.size();
        createInfo.ppEnabledLayerNames = glfwExts.data();
#endif//NDEBUG
        

        if (auto vkRet = vk::createInstance(&createInfo, nullptr, &instance); vkRet != vk::Result::eSuccess) {
            LOGE("Create Vulkan instance faield return code is {}", static_cast<int>(vkRet));
            return MakeGenerateError(AppStatus::FAIL);
        }

        return {};
    }

private:
    vk::Instance instance{};
};

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
    if (auto ret = instance->initialize(); ret) {
        LOGE("inintialize the vulkan instance failed");
        return ret;
    }

    return {};
}

std::error_code Application::run() {
    while (!glfwWindowShouldClose(pwin)) {
        glfwPollEvents();
        glfwSwapBuffers(pwin);
    }

    return {};
}

std::error_code Application::destroy() {
    glfwDestroyWindow(pwin);
    glfwTerminate();
    return {};
}