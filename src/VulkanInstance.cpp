#include <array>
//#include <vulkan/vulkan_structs.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "VulkanInstance.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "ErrorCode.hpp"
#include "Vertext.hpp"
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
#include <chrono>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/hash.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

using namespace Utils;
using namespace Utils::Vulkan;

struct MVPUniformMatrix{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

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
static constexpr const char* kResourcesPath = "/home/ares/home/Code/VulkanLearn/resources";
static constexpr const char* kChaletModelFileName = "chalet.obj";
static constexpr const char* kChaletTextureFileName = "chalet.jpg";
static constexpr const char* kVikingModelFileName = "viking_room.obj";
static constexpr const char* kVikingTextureFileName = "viking_room.png";

std::string GetImageTexurePath(){
    return FileSystem::PathJoin(kResourcesPath, kVikingTextureFileName);
}

std::string GetModelPath(){
    return FileSystem::PathJoin(kResourcesPath, kVikingModelFileName);
}

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
        const auto status = Utils::Vulkan::QuerySwapChainStatus(device, surface);
        swapChainAdequate = !status.formats.empty() && !status.modes.empty();
    }

    auto feat = device.getFeatures();
    return indices && extSupported && swapChainAdequate && feat.samplerAnisotropy;
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

void VulkanInstance::loadModel(){
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, GetModelPath().c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
                _vertices.push_back(vertex);
            }

            _indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void VulkanInstance::cleanSwapChain(){
    _logicDevice->destroyImageView(_depthImageView);
    _logicDevice->destroyImage(_depthImage);
    _logicDevice->freeMemory(_depthImageMemory);

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
    createColorResources();
    createDepthResources();
    createFrameBuffers();
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
    deviceFeat.samplerAnisotropy = vk::True;

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

struct ImageCreateInfo{
    vk::Format format{};
    vk::ImageAspectFlags flag{};
    uint32_t mipLevel{};
};

vk::ImageView CreateImageView(const vk::Device &device, const vk::Image &image, const ImageCreateInfo info){
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = info.format;
    viewInfo.subresourceRange.aspectMask = info.flag;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = info.mipLevel;
    return device.createImageView(viewInfo);
}

void VulkanInstance::createTextureSampler() {
    auto properties = _phyDevice.getProperties();
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.mipLodBias = 0.0f;
    _textureSampler = _logicDevice->createSampler(samplerInfo);
}

void VulkanInstance::createTextureImageView() {
    _textureView = CreateImageView(*_logicDevice, _imageTexture, {vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, _mipLevels});
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
    //createInfo.oldSwapchain = vk::SwapchainKHR(nullptr);
    _swapChain = _logicDevice->createSwapchainKHR(createInfo);
    _swapImages = _logicDevice->getSwapchainImagesKHR(_swapChain);
    _swapForamt = format.format;
    _swapExtent = extent;
}

vk::SampleCountFlagBits GetMaxUsableSampleCount(const vk::PhysicalDevice& physicalDevice) {
    auto physicalDeviceProperties = physicalDevice.getProperties();
    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return  vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

    return vk::SampleCountFlagBits::e1;
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
            _msaaSamples = GetMaxUsableSampleCount(_phyDevice);
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

        vk::DebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
                                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
        debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugInfo.pfnUserCallback = DebugCallback;

        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&DebugCallback;
    }else{
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
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
        _swapChainImageViews[i] = CreateImageView(*_logicDevice, _swapImages[i], {_swapForamt, vk::ImageAspectFlagBits::eColor, 1});
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
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    vk::PipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = _msaaSamples;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
        
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
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &_descSetLayout;
    _renderLayout = _logicDevice->createPipelineLayout(pipelineLayoutInfo);


    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

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
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicState;

    _renderPipeline = _logicDevice->createGraphicsPipeline(nullptr, pipelineInfo).value;
}

void VulkanInstance::createFrameBuffers(){
    _framebuffers.resize(_swapChainImageViews.size());

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        std::array<vk::ImageView, 3> attachments = {
            _colorImageView,
            _depthImageView,
            _swapChainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = _swapExtent.width;
        framebufferInfo.height = _swapExtent.height;
        framebufferInfo.layers = 1;
        _framebuffers[i] = _logicDevice->createFramebuffer(framebufferInfo);
    }
}

vk::Format FindSupportedFormat(const vk::PhysicalDevice &device, const std::vector<vk::Format>& candidates, const vk::ImageTiling tiling, const vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        auto props = device.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format FindDepthFormat(const vk::PhysicalDevice &device) {
    return FindSupportedFormat(device,
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

bool HasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void VulkanInstance::createRenderPass(){
    vk::AttachmentDescription colorAttachment = {};
    colorAttachment.format = _swapForamt;
    colorAttachment.samples = _msaaSamples;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat(_phyDevice);
    depthAttachment.samples = _msaaSamples;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentDescription colorAttachmentResolve = {};
    colorAttachmentResolve.format = _swapForamt;
    colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
    colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachmentResolve.finalLayout = vk::ImageLayout::ePresentSrcKHR;


    vk::AttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1; // 假设深度附件是第二个附件
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    
    std::array<vk::AttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
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
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
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

vk::CommandBuffer SingleTimeCommandBegin(const vk::CommandPool cmdPool, const vk::Device& device){
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = cmdPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);
    return commandBuffer;
}

void SingleTimeCommandEnd(const vk::CommandPool cmdPool, const vk::Device& device, const vk::CommandBuffer &commandBuffer, const vk::Queue &queue){
    commandBuffer.end();
    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    queue.submit(submitInfo, nullptr);
    queue.waitIdle();

    device.freeCommandBuffers(cmdPool, commandBuffer);
}

void VulkanInstance::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    auto commandBuffer = SingleTimeCommandBegin(_cmdPool, *_logicDevice);

    vk::BufferCopy copyRegion = {};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    SingleTimeCommandEnd(_cmdPool, *_logicDevice, commandBuffer, _graphicsQueue);
}

struct Size{
    uint32_t width;
    uint32_t height;
};

struct CommandContext{
    vk::CommandPool cmdPool;
    vk::Device device; 
    vk::Queue queue;
    vk::PhysicalDevice phyDevice;
};

void CopyBuffer2Image(const vk::Buffer &buffer, vk::Image &image, const Size &sz, const CommandContext &context){
    vk::CommandBuffer cb = SingleTimeCommandBegin(context.cmdPool, context.device);
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{
        sz.width,
        sz.height,
        1
    };

    cb.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    SingleTimeCommandEnd(context.cmdPool, context.device, cb, context.queue);
}

void TransitionImageLayout(const vk::Image& image, const vk::Format format, const vk::ImageLayout& oldLayout, const vk::ImageLayout &newLayout, const CommandContext &context, const uint32_t mlevel){
    vk::CommandBuffer cb = SingleTimeCommandBegin(context.cmdPool, context.device);
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = mlevel;
    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    cb.pipelineBarrier(sourceStage, destinationStage,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier);

    SingleTimeCommandEnd(context.cmdPool, context.device, cb, context.queue);
}

struct ImageParam{
    Size size;
    vk::Format format;
    vk::ImageTiling tiling;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags properties;
    uint32_t mipLevel{};
    vk::SampleCountFlagBits msaaSamples{};
};

auto CreateImage(const ImageParam &param, const CommandContext &context) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = param.size.width;
    imageInfo.extent.height = param.size.height;
    imageInfo.extent.depth = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = param.format;
    imageInfo.tiling = param.tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = param.usage;
    imageInfo.samples = param.msaaSamples;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.mipLevels = param.mipLevel;

    auto image = context.device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = context.device.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(context.phyDevice, memRequirements.memoryTypeBits, param.properties);
    auto imageMemory = context.device.allocateMemory(allocInfo);

    context.device.bindImageMemory(image, imageMemory, 0);
    return std::make_pair(image, imageMemory);
}

void GenerateMipmaps(const vk::Image& image, const ImageParam &param, const CommandContext &context) {
    // Check if image format supports linear blitting
    vk::FormatProperties formatProperties = context.phyDevice.getFormatProperties(param.format);    
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    auto commandBuffer = SingleTimeCommandBegin(context.cmdPool, context.device);

    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = param.size.width;
    int32_t mipHeight = param.size.height;

    for (uint32_t i = 1; i < param.mipLevel; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, 
                                    0, nullptr, 
                                    0, nullptr, 
                                    1, &barrier);

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] = vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);
        
        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, 
                                    0, nullptr, 
                                    0, nullptr, 
                                    1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = param.mipLevel - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, 
                                    0, nullptr, 
                                    0, nullptr, 
                                    1, &barrier);

    SingleTimeCommandEnd(context.cmdPool, context.device, commandBuffer, context.queue);
}

void VulkanInstance::createTextureImage(){
    int width, height, channel;
    auto pixels = stbi_load(GetImageTexurePath().c_str(), &width, &height, &channel, STBI_rgb_alpha);
    if(!pixels){
        throw std::runtime_error("Failed to load image");
    }

    _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    const vk::DeviceSize imageSize = width * height * 4;
    auto [buffer, memory] = CreateBuffer(_phyDevice, *_logicDevice, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void *pdata = _logicDevice->mapMemory(memory, 0, imageSize, {});
    memcpy(pdata, pixels, imageSize);
    _logicDevice->unmapMemory(memory);

    stbi_image_free(pixels);

    ImageParam param;
    param.format = vk::Format::eR8G8B8A8Srgb;
    param.size = Size{(uint32_t)width, (uint32_t)height};
    param.tiling = vk::ImageTiling::eOptimal;
    param.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc;
    param.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    param.mipLevel = _mipLevels;
    param.msaaSamples = vk::SampleCountFlagBits::e1;

    auto context = CommandContext{_cmdPool,
        *_logicDevice,
        _graphicsQueue,
        _phyDevice};

     std::tie(_imageTexture, _imageMemory) = CreateImage(param, context);

    TransitionImageLayout(_imageTexture, param.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, context, _mipLevels);
    CopyBuffer2Image(buffer, _imageTexture, param.size, context);
    //TransitionImageLayout(_imageTexture, param.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, context);\

    _logicDevice->destroyBuffer(buffer);
    _logicDevice->freeMemory(memory);

    GenerateMipmaps(_imageTexture, param, context);
}

void VulkanInstance::createVertexBuffer(){
    vk::DeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();
    vk::Buffer stagingBuffer{};
    vk::DeviceMemory stagingBufferMemory{};
    auto [buffer, bufferMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto data = _logicDevice->mapMemory(bufferMemory, 0, bufferSize);
    memcpy(data, _vertices.data(), bufferSize);
    _logicDevice->unmapMemory(bufferMemory);

    auto [buff, buffMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    vk::MemoryPropertyFlagBits::eDeviceLocal);
    _vertexBuffer = buff;
    _vertexBufferMemory = buffMemory;

    copyBuffer(buffer, _vertexBuffer, bufferSize);
    _logicDevice->destroyBuffer(buffer);
    _logicDevice->freeMemory(bufferMemory);
}

void VulkanInstance::createIndexBuffer(){
    vk::DeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();
    vk::Buffer stagingBuffer{};
    vk::DeviceMemory stagingBufferMemory{};
    auto [buffer, bufferMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto data = _logicDevice->mapMemory(bufferMemory, 0, bufferSize);
    memcpy(data, _indices.data(), bufferSize);
    _logicDevice->unmapMemory(bufferMemory);

    auto [buff, buffMemory] = CreateBuffer(_phyDevice, *_logicDevice, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
    vk::MemoryPropertyFlagBits::eDeviceLocal);
    _indexBuffer = buff;
    _indexMemory = buffMemory;

    copyBuffer(buffer, _indexBuffer, bufferSize);
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
    // for (size_t i = 0; i < _cmdBuffers.size(); i++) {
    //     vk::CommandBufferBeginInfo beginInfo = {};
    //     beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
    //     _cmdBuffers[i].begin(beginInfo);
    //     vk::RenderPassBeginInfo renderPassInfo = {};
    //     renderPassInfo.renderPass = _renderPass;
    //     renderPassInfo.framebuffer = _framebuffers[i];
    //     renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
    //     renderPassInfo.renderArea.extent = _swapExtent;

    //     vk::ClearValue clearColor = { std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } };
    //     renderPassInfo.clearValueCount = 1;
    //     renderPassInfo.pClearValues = &clearColor;

    //     _cmdBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    //     _cmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, _renderPipeline);

    //     vk::Buffer vertexBuffers[] = { _vertexBuffer };
    //     vk::DeviceSize offsets[] = { 0 };
    //     _cmdBuffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);
    //     _cmdBuffers[i].bindIndexBuffer(_indexBuffer, 0, vk::IndexType::eUint16);

    //     //_cmdBuffers[i].draw(3, 1, 0, 0);
    //     _cmdBuffers[i].drawIndexed(_indices.size(), 1, 0, 0, 0);
    //     _cmdBuffers[i].endRenderPass();
    //     _cmdBuffers[i].end();
    // }
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

void VulkanInstance::createDescriptorSetLayout(){
    vk::DescriptorSetLayoutBinding uboLayout{};
    uboLayout.binding = 0;
    uboLayout.descriptorCount = 1;
    uboLayout.pImmutableSamplers = nullptr;
    uboLayout.stageFlags = vk::ShaderStageFlagBits::eVertex;
    uboLayout.descriptorType = vk::DescriptorType::eUniformBuffer;

    vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayoutBinding, 2> binds{uboLayout, samplerLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo info{};
    info.bindingCount = binds.size();
    info.pBindings = binds.data();

    _descSetLayout = _logicDevice->createDescriptorSetLayout(info);
}

void VulkanInstance::createUniformBuffer(){
    vk::DeviceSize size = sizeof(MVPUniformMatrix);
    _mvpBuffer.resize(MAX_FRAMES_IN_FLIGHT);
    _mvpData.resize(MAX_FRAMES_IN_FLIGHT);
    _mvpMemory.resize(MAX_FRAMES_IN_FLIGHT);
    for(auto i = 0;i < MAX_FRAMES_IN_FLIGHT;i ++){
        std::tie(_mvpBuffer[i], _mvpMemory[i]) = CreateBuffer(_phyDevice, _logicDevice.get(), size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        _mvpData[i] = _logicDevice->mapMemory(_mvpMemory[i], 0, size);
    }
}

void VulkanInstance::createDescriptorPool(){
    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
    poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    _descriptorPool = _logicDevice->createDescriptorPool(poolInfo, nullptr);
}

void VulkanInstance::createDescriptorSets(){
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _descSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    _descriptorSets = _logicDevice->allocateDescriptorSets(allocInfo);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = _mvpBuffer[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(MVPUniformMatrix);

        vk::DescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = _textureView;
        imageInfo.sampler = _textureSampler;

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].dstSet = _descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].dstSet = _descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        _logicDevice->updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }
}
void VulkanInstance::createColorResources(){
    ImageParam param;
    param.format = _swapForamt;
    param.size = Size{(uint32_t)_swapExtent.width, (uint32_t)_swapExtent.height};
    param.tiling = vk::ImageTiling::eOptimal;
    param.usage = vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment;
    param.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    param.mipLevel = 1;
    param.msaaSamples = _msaaSamples;
    
    auto context = CommandContext{_cmdPool,
        *_logicDevice,
        _graphicsQueue,
        _phyDevice};
    std::tie(_colorImage, _colorImageMemory) = CreateImage(param, context);
    _colorImageView = CreateImageView(*_logicDevice, _colorImage, {param.format, vk::ImageAspectFlagBits::eColor,1});
}

void VulkanInstance::createDepthResources(){
    const auto depthFormat = FindDepthFormat(_phyDevice);
    ImageParam param;
    param.format = depthFormat;
    param.size = Size{(uint32_t)_swapExtent.width, (uint32_t)_swapExtent.height};
    param.tiling = vk::ImageTiling::eOptimal;
    param.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    param.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    param.mipLevel = 1;
    param.msaaSamples = _msaaSamples;

    auto context = CommandContext{_cmdPool,
        *_logicDevice,
        _graphicsQueue,
        _phyDevice};
    std::tie(_depthImage, _depthImageMemory) = CreateImage(param, context);
    _depthImageView = CreateImageView(*_logicDevice, _depthImage, {depthFormat, vk::ImageAspectFlagBits::eDepth,1});
}

void VulkanInstance::updateUniformBuffer(const uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count() * 0.1;

    MVPUniformMatrix ubo = {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), _swapExtent.width / (float) _swapExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;  // Vulkan Y coordinate correction

    // Copy data to the mapped memory
    memcpy(_mvpData[currentImage], &ubo, sizeof(ubo));
}

std::error_code VulkanInstance::initialize(GLFWwindow *window, const uint32_t width, const uint32_t height) {
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FrameBufferResizedCallback);
    try{
        loadModel();
        createInstance();
        setupDebugCallback();
        createSurface(window);
        SelectRunningDevice();
        createLogicDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createColorResources();
        createDepthResources();
        createFrameBuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffer();
        createSyncObject();
    }catch(const std::runtime_error &err){
        destroy();
        return std::make_error_code(std::errc::operation_canceled);
    }

    return {};
}

void VulkanInstance::destroy(){
    if(!_instance) return;
    cleanSwapChain();
    _logicDevice->destroyImageView(_colorImageView);
    _logicDevice->destroyImage(_colorImage);
    _logicDevice->freeMemory(_colorImageMemory);
    _logicDevice->destroyBuffer(_indexBuffer);
    _logicDevice->freeMemory(_indexMemory);
    _logicDevice->destroyBuffer(_vertexBuffer);
    _logicDevice->freeMemory(_vertexBufferMemory);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _logicDevice->destroySemaphore(_renderFinishedSemaphores[i]);
        _logicDevice->destroySemaphore(_imageAvailableSemaphores[i]);
        _logicDevice->destroyFence(_inFlightFences[i]);
        _logicDevice->destroyBuffer(_mvpBuffer[i]);
        _logicDevice->freeMemory(_mvpMemory[i]);
    }

    _logicDevice->destroySampler(_textureSampler);
    _logicDevice->destroyImageView(_textureView);
    _logicDevice->destroyDescriptorPool(_descriptorPool);
    _logicDevice->destroyImage(_imageTexture);
    _logicDevice->freeMemory(_imageMemory);
    _logicDevice->destroyDescriptorSetLayout(_descSetLayout);
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

void VulkanInstance::recordCommandBuffer(const uint32_t imageIndex){
    vk::CommandBufferBeginInfo beginInfo = {};
    const auto cmdBuffer = _cmdBuffers[_currentFrame];
    cmdBuffer.begin(beginInfo);

    {
        vk::RenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.renderPass = _renderPass;
        renderPassInfo.framebuffer = _framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
        renderPassInfo.renderArea.extent = _swapExtent;

        std::array<vk::ClearValue, 2> clearColor{};
        clearColor[0].color = vk::ClearColorValue{std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}};
        clearColor[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

        renderPassInfo.clearValueCount = clearColor.size();
        renderPassInfo.pClearValues = clearColor.data();

        cmdBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _renderPipeline);
        {
            vk::Viewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) _swapExtent.width;
            viewport.height = (float) _swapExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vk::Rect2D scissor{};
            scissor.offset = vk::Offset2D{0, 0};
            scissor.extent = _swapExtent;

            cmdBuffer.setViewport(0, 1, &viewport);
            cmdBuffer.setScissor(0, 1, &scissor);
        }

        {
            vk::Buffer vertexBuffers[] = { _vertexBuffer };
            vk::DeviceSize offsets[] = { 0 };
            cmdBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
            cmdBuffer.bindIndexBuffer(_indexBuffer, 0, vk::IndexType::eUint32);
            cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _renderLayout, 0, _descriptorSets[_currentFrame], {});
            //_cmdBuffers[i].draw(3, 1, 0, 0);
            cmdBuffer.drawIndexed(_indices.size(), 1, 0, 0, 0);
        }
        cmdBuffer.endRenderPass();
    }

    cmdBuffer.end();
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
    
    updateUniformBuffer(_currentFrame);
    [[maybe_unused]]auto r = _logicDevice->resetFences(1, &_inFlightFences[_currentFrame]);
    _cmdBuffers[_currentFrame].reset();
    recordCommandBuffer(imageIndex);

    vk::SubmitInfo submitInfo = {};
    vk::Semaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_cmdBuffers[_currentFrame];

    vk::Semaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    
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