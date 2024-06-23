#pragma once
#include "Application.hpp"
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

class VulkanInstance {
public:
    ~VulkanInstance(){
        destroy();
    }
    
public:
    void destroy();
    std::error_code initialize(GLFWwindow *window, const uint32_t width = 0, const uint32_t height = 0);
    void draw();
    void wait();
    
private:
    void createInstance();
	void setupDebugCallback();
    void SelectRunningDevice();
    void createLogicDevice();
    void createSurface(GLFWwindow *window);
    void createSwapChain();
    void createImageViews();
    void createGraphicsPipeline();
    void createRenderPass();
    void createFrameBuffers();
    void createCommandBuffer();
    void createVertexBuffer();
    void createIndexBuffer();
    void createCommandPool();
    void createSyncObject();
    void cleanSwapChain();
    void recreateSwapChain();
    void createUniformBuffer();
    void createDescriptorSetLayout();
    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
    void updateUniformBuffer(const uint32_t currentImage);
    void recordCommandBuffer(const uint32_t index);
    void createDescriptorPool();
    void createDescriptorSets();
public:
    bool _frameBufferResized{false};
    
private:
    vk::UniqueInstance _instance{};
    vk::PhysicalDevice _phyDevice{};
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
    vk::RenderPass _renderPass{};
    vk::PipelineLayout _renderLayout{};
    vk::Pipeline _renderPipeline{};
    std::vector<vk::Framebuffer> _framebuffers;
    vk::CommandPool _cmdPool{};
    std::vector<vk::CommandBuffer, std::allocator<vk::CommandBuffer>> _cmdBuffers{};
    uint32_t _width{};
    uint32_t _height{};
    std::vector<vk::Semaphore> _imageAvailableSemaphores;
    std::vector<vk::Semaphore> _renderFinishedSemaphores;
    std::vector<vk::Fence> _inFlightFences;
    size_t _currentFrame = 0;
    vk::Buffer _vertexBuffer{};
    vk::DeviceMemory _vertexBufferMemory{};
    vk::Buffer _indexBuffer{};
    vk::DeviceMemory _indexMemory{};
    vk::DescriptorSetLayout _descSetLayout{};
    std::vector<vk::Buffer> _mvpBuffer{};
    std::vector<vk::DeviceMemory> _mvpMemory{};
    std::vector<void*> _mvpData{};
    vk::DescriptorPool _descriptorPool{};
    std::vector<vk::DescriptorSet> _descriptorSets{};
};