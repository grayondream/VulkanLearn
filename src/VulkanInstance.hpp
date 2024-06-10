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
	void setupDebugCallback();

private:
    vk::Instance instance{};
    VkDebugUtilsMessengerEXT callback;
};