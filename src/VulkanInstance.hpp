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
    std::error_code initialize(GLFWwindow *window, const uint32_t width = 0, const uint32_t height = 0);

private:
    void createInstance();
	void setupDebugCallback();
    void SelectRunningDevice();
    void createLogicDevice();
    void createSurface(GLFWwindow *window);
    void createSwapChain();
    void createImageViews();
    
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
    vk::SwapchainKHR _swapChain{};
    std::vector<vk::Image> _swapImages{};
    std::vector<vk::ImageView> _swapChainImageViews;
    vk::Format _swapForamt{};
    vk::Extent2D _swapExtent{};
    uint32_t _width{};
    uint32_t _height{};
};