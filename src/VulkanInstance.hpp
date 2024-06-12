#pragma once
#include "Application.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

class VulkanInstance {
public:
    ~VulkanInstance(){
        destroy();
    }
    
public:
    void destroy();
    std::error_code initialize(GLFWwindow *window);

private:
	void setupDebugCallback();
    void SelectRunningDevice();
    void createLogicDevice();
    void createSurface(GLFWwindow *window);

private:
    vk::Instance _instance{};
    vk::PhysicalDevice _phyDevice{};
    int32_t _phyDeviceIndex{};
    vk::UniqueDevice _logicDevice{};
    vk::Queue _graphicsQueue{};
    vk::SurfaceKHR _surface;
    vk::Queue _presentQueue{};
    GLFWwindow* _pwindows{};
    VkDebugUtilsMessengerEXT callback;
};