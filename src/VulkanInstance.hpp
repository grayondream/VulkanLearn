#pragma once
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
    std::error_code initialize();

private:
	void setupDebugCallback();
    void SelectRunningDevice();
    void createLogicDevice();

private:
    vk::Instance instance{};
    vk::PhysicalDevice _phyDevice{};
    int32_t _phyDeviceIndex{};
    vk::UniqueDevice _logicDevice{};
    vk::Queue _graphicsQueue{};
    VkDebugUtilsMessengerEXT callback;
};