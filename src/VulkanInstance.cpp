#include "VulkanInstance.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "ErrorCode.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <bits/types/wint_t.h>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <set>
#include <glm/glm.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include "Vertext.hpp"

using namespace Utils;
using namespace Utils::Vulkan;

static constexpr const int MAX_FRAMES_IN_FLIGHT = 2;
const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifndef NDEBUG
static constexpr const bool gEnableValidationLayer = false;
#else
static constexpr const bool gEnableValidationLayer = false;
#endif//NDEBUG

static constexpr const char* kShaderPath = "/home/ares/home/Code/VulkanLearn/shader";

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<Vertex> gVertics = {
    {{0.5,-0.5},{1.0,0,0.0}},
    {{0.5,0.5},{0.0,1.0,0.0}},
    {{-0.5,0.5},{0.0,0.0,1.0}}
};

static std::error_code CheckPrintVKExtensions(){
    auto vkExts = Vulkan::QueryVkExtensions();
    if (vkExts.empty()) {
        LOGE("Can not get any extensions from the local device!");
        return MakeGenerateError(AppStatus::FAIL);
    }

    for (auto i = 0; i < vkExts.size(); i++) {
        const auto e = vkExts[i];
        LOGI("The {} extension,version:{}, name:{}", i, e.specVersion, e.extensionName);
    }

    return {};
}

VkBool32 DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData){
    static std::once_flag oflag;
    std::call_once(oflag, [](){
        LOGD("Debug Callback has been called");
    });
    return true;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance _instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
    auto pf = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
    if(!pf){
        LOGE("Failed to load the vkCreateDebugUtilsMessengerEXT");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }


    return pf(_instance, pCreateInfo, pAllocator, pCallback);
}

void DestroyDebugUtilsMessengerEXT(VkInstance _instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(_instance, callback, pAllocator);
    }
}

inline static bool CheckDeviceSuitable(const vk::PhysicalDevice& device, const vk::SurfaceKHR &surface){
    const auto indices = Utils::Vulkan::QueryQueueFamilyIndices(device, surface).isComplete();
    bool extSupported = Vulkan::CheckDeviceExtensionSupport(device, kDeviceExtensions);
    bool swapChainAdequate{false};
    if(extSupported){
        VKSwapChainSupportStatus status = Utils::Vulkan::QuerySwapChainStatus(device, surface);
        swapChainAdequate = !status.formats.empty() && !status.modes.empty();
    }

    return indices && extSupported && swapChainAdequate;
}

inline static vk::Extent2D ChooseSwapExtend(const vk::SurfaceCapabilitiesKHR &capas, const uint32_t width, const uint32_t height){
    if(capas.currentExtent.width != std::numeric_limits<uint32_t>::max()){
        return capas.currentExtent;
    }else{
        vk::Extent2D extent = { width, height};
        extent.width = std::max(capas.minImageExtent.width, std::min(capas.maxImageExtent.width, extent.width));
        extent.height = std::max(capas.minImageExtent.height, std::min(capas.maxImageExtent.height, extent.height));
        return extent;
    }
}

vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes) {
    vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
            bestMode = availablePresentMode;
        }
    }

    return bestMode;
}

vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
        return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
    }

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

void VulkanInstance::cleanSwapChain(){
    for (auto framebuffer : _framebuffers) {
        _logicDevice->destroyFramebuffer(framebuffer);
    }

    _logicDevice->freeCommandBuffers(_cmdPool, _cmdBuffers);
    _logicDevice->destroyPipeline(_renderPipeline);
    _logicDevice->destroyPipelineLayout(_renderLayout);
    _logicDevice->destroyRenderPass(_renderPass);
    for(auto && view : _swapChainImageViews){
        _logicDevice->destroyImageView(view);
    }

    if(_swapChain && _logicDevice){
        _logicDevice->destroySwapchainKHR(_swapChain);
    }
}

void VulkanInstance::recreateSwapChain(){
    int width = 0, height = 0;
    while(width == 0 || height == 0){
        glfwGetFramebufferSize(_pwindows, &width, &height);
        glfwWaitEvents();
    }

    _logicDevice->waitIdle();
    cleanSwapChain();
    createSwapChain();
    createImageViews();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFrameBuffers();
    createCommandBuffer();
}

void VulkanInstance::createLogicDevice(){
    float priority = 1.0;
    auto indics = Utils::Vulkan::QueryQueueFamilyIndices(_phyDevice, _surface);
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};
    std::set<uint32_t> queueFamilies = { indics.graphics.value(), indics.present.value() };
    for(auto fam : queueFamilies){
        queueCreateInfos.emplace_back(
            vk::DeviceQueueCreateFlags(), 
            fam, 
            1, 
            &priority);
    }

    auto deviceFeat = vk::PhysicalDeviceFeatures();
    auto createInfo = vk::DeviceCreateInfo(
        vk::DeviceCreateFlags(),
        queueCreateInfos.size(),
        queueCreateInfos.data()
    );
    createInfo.pEnabledFeatures = &deviceFeat;
    createInfo.enabledExtensionCount = kDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
    if(gEnableValidationLayer){
        createInfo.enabledLayerCount = kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    _logicDevice = _phyDevice.createDeviceUnique(createInfo);
    _graphicsQueue = _logicDevice->getQueue(indics.graphics.value(), 0);
    _presentQueue = _logicDevice->getQueue(indics.present.value(), 0);
}

void VulkanInstance::createSwapChain(){
    VKSwapChainSupportStatus status = Utils::Vulkan::QuerySwapChainStatus(_phyDevice, _surface);
    auto format = ChooseSwapSurfaceFormat(status.formats);
    auto mode = ChooseSwapPresentMode(status.modes);
    auto extent = ChooseSwapExtend(status.capas, _width, _height);
    uint32_t imgCount = status.capas.minImageCount + 1;
    if(status.capas.maxImageCount > 0 && imgCount > status.capas.maxImageCount){
        imgCount = status.capas.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{
        vk::SwapchainCreateFlagsKHR(),
        _surface,
        imgCount,
        format.format,
        format.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment
    };

    auto indics = QueryQueueFamilyIndices(_phyDevice, _surface);
    uint32_t famIndics[] = { indics.graphics.value(), indics.present.value()};
    if(indics.graphics != indics.present){
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = famIndics;
    }else{
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }
    createInfo.preTransform = status.capas.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = vk::SwapchainKHR(nullptr);
    _swapChain = _logicDevice->createSwapchainKHR(createInfo);
    _swapImages = _logicDevice->getSwapchainImagesKHR(_swapChain);
    _swapForamt = format.format;
    _swapExtent = extent;
}

void VulkanInstance::SelectRunningDevice(){
    auto devices = _instance->enumeratePhysicalDevices();
    if(devices.empty()){
        throw std::runtime_error("No Physical Device found");
    }

    for(auto &&device : devices){
        //LOGI("The {}th device is {}", i, devices[i].)
        if(CheckDeviceSuitable(device, _surface)){
            _phyDevice = device;
            break;//only find one device;
        }
    }

    if(!_phyDevice){
        throw std::runtime_error("The device is nullptr, can not find any suitable device");
    }

    auto phyDevicePro = _phyDevice.getProperties();
    LOGI("Select Device Id {} Device name {}", phyDevicePro.deviceID, std::string(phyDevicePro.deviceName));
}

void VulkanInstance::createInstance(){
    if(auto ret = CheckPrintVKExtensions(); ret){
        LOGE("Failed to load the vulkan extensions");
        throw std::runtime_error("Can not find any vulkan extensions");
    }
     
    if (gEnableValidationLayer && !CheckValidationLayerSupport(kValidationLayers)) {
        LOGE("The device donot support validation layer provided!");
        throw std::runtime_error("Enable Validation Layer, but can not find any supported validate layer!");
    }

    vk::ApplicationInfo appInfo = { 
        "Hello Vulkan", 
        VK_MAKE_VERSION(1, 0, 0), 
        "Everything but engine", 
        VK_MAKE_VERSION(1, 0, 0), 
        VK_API_VERSION_1_0 };   
        
    auto glfwExts = Vulkan::QueryGlfwExtension();
    vk::InstanceCreateInfo createInfo = { 
        vk::InstanceCreateFlags{}, 
        &appInfo,
        0,
        nullptr,
        (uint32_t)glfwExts.size(),
        glfwExts.data()
    };
    if(gEnableValidationLayer){
        createInfo.enabledLayerCount = kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    for(auto&&l : kValidationLayers){
        LOGI("Enable the validation layer {}", l);
    }

    for(auto&& name : glfwExts){
        LOGI("Enable extension {}", name);
    }

    _instance = vk::createInstanceUnique(createInfo, nullptr);
}

void VulkanInstance::createSurface(GLFWwindow *window){
    VkSurfaceKHR surface{};
    if(VK_SUCCESS != glfwCreateWindowSurface(*_instance, window, nullptr, &surface)){
        throw std::runtime_error("Failed to create window surface");
    }

    _surface = surface;
}

void VulkanInstance::setupDebugCallback(){
    if(!gEnableValidationLayer) {
        return;
    }

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{
        vk::DebugUtilsMessengerCreateFlagsEXT(),
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
        DebugCallback,
        nullptr
    };
    if(CreateDebugUtilsMessengerEXT(*_instance, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo), nullptr, &callback)){
        throw std::runtime_error("Failed to register debug utils");
    }
}


void VulkanInstance::createImageViews(){
    _swapChainImageViews.resize(_swapImages.size());
    for (size_t i = 0; i < _swapImages.size(); i++) {
        vk::ImageViewCreateInfo createInfo = {};
        createInfo.image = _swapImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = _swapForamt;
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        _swapChainImageViews[i] = _logicDevice->createImageView(createInfo);
    }
}

const vk::UniqueShaderModule CreateShaderModule(const vk::Device device,const std::vector<char> &code){
    return device.createShaderModuleUnique({
        vk::ShaderModuleCreateFlags(),
        code.size(),
        reinterpret_cast<const uint32_t*>(code.data())
    });
}

void VulkanInstance::createGraphicsPipeline(){
    auto vertShaderStr = Utils::FileSystem::ReadFile(FileSystem::PathJoin(kShaderPath, "vert.spv"));
    auto fragShaderStr = Utils::FileSystem::ReadFile(FileSystem::PathJoin(kShaderPath, "frag.spv"));
    LOGD("Vertex Shader:{}", vertShaderStr.size());
    LOGD("Fragment Shader:{}", fragShaderStr.size());
    auto vertModule = CreateShaderModule(*_logicDevice, vertShaderStr);
    auto fragModule = CreateShaderModule(*_logicDevice, fragShaderStr);
    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        {
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            *vertModule,
            "main"
        },
        {
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eFragment,
            *fragModule,
            "main"
        }
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    auto bindDesc = Vertex::getBindingDesc();
    auto attDesc = Vertex::getAttributeDesc();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = attDesc.size();
    vertexInputInfo.pVertexAttributeDescriptions = attDesc.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)_swapExtent.width;
    viewport.height = (float)_swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor = {};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = _swapExtent;

    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
    _renderLayout = _logicDevice->createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = _renderLayout;
    pipelineInfo.renderPass = _renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr;
    _renderPipeline = _logicDevice->createGraphicsPipeline(nullptr, pipelineInfo).value;
}

void VulkanInstance::createFrameBuffers(){
    _framebuffers.resize(_swapChainImageViews.size());

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        vk::ImageView attachments[] = {
            _swapChainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = _swapExtent.width;
        framebufferInfo.height = _swapExtent.height;
        framebufferInfo.layers = 1;
        _framebuffers[i] = _logicDevice->createFramebuffer(framebufferInfo);
    }
}


void VulkanInstance::createRenderPass(){
    vk::AttachmentDescription colorAttachment = {};
    colorAttachment.format = _swapForamt;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    //dependency.srcAccessMask = {};
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    
    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    _renderPass = _logicDevice->createRenderPass(renderPassInfo);
}

void VulkanInstance::createCommandPool(){
    auto queueFamilyIndices = Vulkan::QueryQueueFamilyIndices(_phyDevice, _surface);
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphics.value();
    _cmdPool = _logicDevice->createCommandPool(poolInfo);
}

uint32_t FindMemoryType(const vk::PhysicalDevice &physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(const vk::PhysicalDevice &pdevice, const vk::Device device, const vk::DeviceSize size, const vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags properFlags){
    vk::BufferCreateInfo info{};
    info.size = size;
    info.usage = usageFlags;
    info.sharingMode = vk::SharingMode::eExclusive;
    auto buffer = device.createBuffer(info);
    vk::MemoryRequirements requieMents = device.getBufferMemoryRequirements(buffer);
    vk::MemoryAllocateInfo mInfo{};
    mInfo.allocationSize = requieMents.size;
    mInfo.memoryTypeIndex = FindMemoryType(pdevice, requieMents.memoryTypeBits, properFlags);
    auto bufferMemory = device.allocateMemory(mInfo);
    device.bindBufferMemory(buffer, bufferMemory, 0);
    return {buffer, bufferMemory};
}

void VulkanInstance::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = _cmdPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer = _logicDevice->allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

        vk::BufferCopy copyRegion = {};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    commandBuffer.end();

    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    _graphicsQueue.submit(submitInfo, nullptr);
    _graphicsQueue.waitIdle();

    _logicDevice->freeCommandBuffers(_cmdPool, commandBuffer);
}

void VulkanInstance::createVertexBuffer(){
    vk::DeviceSize bufferSize = sizeof(gVertics[0]) * gVertics.size();
    vk::Buffer stagingBuffer{};
    vk::DeviceMemory stagingBufferMemory{};
    auto [buffer, bufferMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto data = _logicDevice->mapMemory(bufferMemory, 0, bufferSize);
    memcpy(data, gVertics.data(), bufferSize);
    
    auto [buff, buffMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    _vertexBuffer = buff;
    _vertexBufferMemory = buffMemory;

    copyBuffer(buffer, _vertexBuffer, bufferSize);
    _logicDevice->destroyBuffer(buffer);
    _logicDevice->freeMemory(bufferMemory);
}

void VulkanInstance::createCommandBuffer(){
    _cmdBuffers.resize(_framebuffers.size());

    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = _cmdPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = (uint32_t)_cmdBuffers.size();

    _cmdBuffers = _logicDevice->allocateCommandBuffers(allocInfo);
    for (size_t i = 0; i < _cmdBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo = {};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        _cmdBuffers[i].begin(beginInfo);
        vk::RenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.renderPass = _renderPass;
        renderPassInfo.framebuffer = _framebuffers[i];
        renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
        renderPassInfo.renderArea.extent = _swapExtent;

        vk::ClearValue clearColor = { std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        _cmdBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        _cmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, _renderPipeline);

        vk::Buffer vertexBuffers[] = { _vertexBuffer };
        vk::DeviceSize offsets[] = { 0 };
        _cmdBuffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);

        _cmdBuffers[i].draw(3, 1, 0, 0);
        _cmdBuffers[i].endRenderPass();
        _cmdBuffers[i].end();
    }
}

void VulkanInstance::createSyncObject(){
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _imageAvailableSemaphores[i] = _logicDevice->createSemaphore({});
        _renderFinishedSemaphores[i] = _logicDevice->createSemaphore({});
        _inFlightFences[i] = _logicDevice->createFence({vk::FenceCreateFlagBits::eSignaled});
    }
}

static void FrameBufferResizedCallback(GLFWwindow* pwin, int width, int height){
    auto app = reinterpret_cast<VulkanInstance*>(glfwGetWindowUserPointer(pwin));
    app->_frameBufferResized = true;
}

std::error_code VulkanInstance::initialize(GLFWwindow *window, const uint32_t width, const uint32_t height) {
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FrameBufferResizedCallback);
    createInstance();
    setupDebugCallback();
    createSurface(window);
    SelectRunningDevice();
    createLogicDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFrameBuffers();
    createCommandPool();
    createVertexBuffer();
    createCommandBuffer();
    createSyncObject();
    return {};
}

void VulkanInstance::destroy(){
    if(!_instance) return;
    cleanSwapChain();
    _logicDevice->destroyBuffer(_vertexBuffer);
    _logicDevice->freeMemory(_vertexBufferMemory);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _logicDevice->destroySemaphore(_renderFinishedSemaphores[i]);
        _logicDevice->destroySemaphore(_imageAvailableSemaphores[i]);
        _logicDevice->destroyFence(_inFlightFences[i]);
    }

    _logicDevice->destroyCommandPool(_cmdPool);    
    if(_logicDevice){
        _logicDevice->waitIdle();
        _logicDevice.reset();
    }

    if(_surface){
        _instance->destroySurfaceKHR(_surface);
        _surface = nullptr;
    }

    _phyDevice = nullptr;
    if(gEnableValidationLayer){
        DestroyDebugUtilsMessengerEXT(*_instance, callback, nullptr);
        callback = nullptr;
    }

    _instance.reset();
}

void VulkanInstance::draw(){
    [[maybe_unused]]auto t = _logicDevice->waitForFences(1, &_inFlightFences[_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    uint32_t imageIndex{};
    try{
        imageIndex = _logicDevice->acquireNextImageKHR(_swapChain, std::numeric_limits<uint64_t>::max(), 
        _imageAvailableSemaphores[_currentFrame], nullptr).value;
    }catch(vk::OutOfDateKHRError &err){
        recreateSwapChain();
        return;
    }
    
    vk::SubmitInfo submitInfo = {};

    vk::Semaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_cmdBuffers[imageIndex];

    vk::Semaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    [[maybe_unused]]auto r = _logicDevice->resetFences(1, &_inFlightFences[_currentFrame]);
    _graphicsQueue.submit(submitInfo, _inFlightFences[_currentFrame]);

    vk::PresentInfoKHR presentInfo = {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    vk::SwapchainKHR swapChains[] = { _swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    r = _presentQueue.presentKHR(presentInfo);
    if(r == vk::Result::eSuboptimalKHR || _frameBufferResized){
        recreateSwapChain();
        _frameBufferResized = false;
        return;
    }
    
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanInstance::wait(){
    _logicDevice->waitIdle();
}