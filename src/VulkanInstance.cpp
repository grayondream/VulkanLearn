#include "VulkanInstance.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "ErrorCode.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <set>

#ifndef NDEBUG

using namespace Utils;
using namespace Utils::Vulkan;

static constexpr const bool gEnableValidationLayer = true;
const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#else
static constexpr const bool gEnableValidationLayer = false;
#endif//NDEBUG

static constexpr const char* kShaderPath = "/home/ares/home/Code/VulkanLearn/shader";

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

void VulkanInstance::createLogicDevice(){
    float priority = 1.0;
    auto indics = Utils::Vulkan::QueryQueueFamilyIndices(_phyDevice, _surface);
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};
    std::set<uint32_t> queueFamilies = { indics.graphics.value(), indics.present.value() };
    for(auto fam : queueFamilies){
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), fam, 1, &priority);
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
    try{
        _logicDevice = _phyDevice.createDeviceUnique(createInfo);
    }catch(vk::SystemError &err){
        LOGE("Failed to create vulkan logic device, message is {}", err.what());
        throw std::runtime_error(err.what());
    }

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
    try {
        _swapChain = _logicDevice->createSwapchainKHR(createInfo);
    }
    catch (vk::SystemError err) {
        throw std::runtime_error("failed to create swap chain!");
    }

    _swapImages = _logicDevice->getSwapchainImagesKHR(_swapChain);
    _swapForamt = format.format;
    _swapExtent = extent;
}

void VulkanInstance::SelectRunningDevice(){
    auto devices = _instance.enumeratePhysicalDevices();
    if(devices.empty()){
        throw std::runtime_error("No Physical Device found");
    }

    for(auto i = 0;i < devices.size();i ++){
        //LOGI("The {}th device is {}", i, devices[i].)
        if(CheckDeviceSuitable(devices[i], _surface)){
            _phyDevice = devices[i];
            _phyDeviceIndex = i;
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

    vk::ApplicationInfo appInfo = { "Hello Vulkan", VK_MAKE_VERSION(1, 0, 0), "Everything but engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0 };        
    if (gEnableValidationLayer && !CheckValidationLayerSupport(kValidationLayers)) {
        LOGE("The device donot support validation layer provided!");
        throw std::runtime_error("Enable Validation Layer, but can not find any supported validate layer!");
    }

    auto glfwExts = Vulkan::QueryGlfwExtension();
    glfwExts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    vk::InstanceCreateInfo createInfo = { vk::InstanceCreateFlags{}, &appInfo };
    createInfo.enabledLayerCount = 0;
    if(gEnableValidationLayer){
        createInfo.enabledLayerCount = kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        createInfo.enabledExtensionCount = glfwExts.size();
        createInfo.ppEnabledExtensionNames = glfwExts.data();
    }

    for(auto&&l : kValidationLayers){
        LOGI("Enable the validation layer {}", l);
    }

    for(auto&& name : glfwExts){
        LOGI("Enable extension {}", name);
    }

    if (auto vkRet = vk::createInstance(&createInfo, nullptr, &_instance); vkRet != vk::Result::eSuccess) {
        LOGE("Create Vulkan _instance faield return code is {}", static_cast<int>(vkRet));
        std::runtime_error("Failed to create vulkan _instance");
    }
}

void VulkanInstance::createSurface(GLFWwindow *window){
    VkSurfaceKHR surface{};
    if(VK_SUCCESS != glfwCreateWindowSurface(_instance, window, nullptr, &surface)){
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
    if(CreateDebugUtilsMessengerEXT(_instance, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo), nullptr, &callback)){
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

        try {
            _swapChainImageViews[i] = _logicDevice->createImageView(createInfo);
        }
        catch (vk::SystemError err) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

static vk::UniqueShaderModule CreateShaderModule(const vk::UniqueDevice &device, const std::vector<char> &code){
    try{
        return device->createShaderModuleUnique(
            {
                vk::ShaderModuleCreateFlags(),
                code.size(),
                (uint32_t*)code.data(),
            }
        );
    }catch(vk::SystemError &err){
        throw std::runtime_error("Failed to crate shader module!");
    }
}

void VulkanInstance::createGraphicsPipeline(){
    auto vertShaderStr = Utils::FileSystem::ReadFile(FileSystem::PathJoin(kShaderPath, "vert.spv"));
    auto fragShaderStr = Utils::FileSystem::ReadFile(FileSystem::PathJoin(kShaderPath, "frag.spv"));
    LOGD("Vertex Shader:{}", vertShaderStr.size());
    LOGD("Fragment Shader:{}", fragShaderStr.size());
    vk::PipelineShaderStageCreateInfo renderStage[] = {
        {
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            *CreateShaderModule(_logicDevice, vertShaderStr),
            "main"
        },
        {
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eFragment,
            *CreateShaderModule(_logicDevice, fragShaderStr),
            "main"
        }
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

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
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    try {
        _renderLayout = _logicDevice->createPipelineLayout(pipelineLayoutInfo);
    } catch (vk::SystemError err) {
        throw std::runtime_error("failed to create pipeline layout!");
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

    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    try {
        _renderPass = _logicDevice->createRenderPass(renderPassInfo);
    } catch (vk::SystemError err) {
        throw std::runtime_error("failed to create render pass!");
    }
}

std::error_code VulkanInstance::initialize(GLFWwindow *window, const uint32_t width, const uint32_t height) {
    createInstance();
    setupDebugCallback();
    createSurface(window);
    SelectRunningDevice();
    createLogicDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createRenderPass();
    return {};
}

void VulkanInstance::destroy(){
    if(!_instance) return;
    for(auto && view : _swapChainImageViews){
        _logicDevice->destroyImageView(view);
    }

    if(_logicDevice){
        _logicDevice->waitIdle();
        _logicDevice.reset();
    }

    if(_swapChain && _logicDevice){
        _logicDevice->destroySwapchainKHR(_swapChain);
    }
    if(_surface){
        _instance.destroySurfaceKHR(_surface);
        _surface = nullptr;
    }

    _phyDevice = nullptr;
    if(gEnableValidationLayer){
        DestroyDebugUtilsMessengerEXT(_instance, callback, nullptr);
        callback = nullptr;
    }

    _instance = nullptr;
}