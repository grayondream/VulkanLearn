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

private:
    vk::Instance instance{};
    vk::PhysicalDevice _phyDevice{};
    VkDebugUtilsMessengerEXT callback;
};