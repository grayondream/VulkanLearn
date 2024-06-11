#include "VulkanInstance.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "ErrorCode.hpp"
#include <stdexcept>
#include <system_error>
#include <vulkan/vulkan.hpp>
#ifndef NDEBUG

using namespace Utils;
static constexpr const bool gEnableValidationLayer = true;
const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#else
static constexpr const bool gEnableValidationLayer = false;
#endif//NDEBUG

using namespace Utils::Vulkan;


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
    LOGD("Debug Callback has been called");
    return true;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
    auto pf = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(!pf){
        LOGE("Failed to load the vkCreateDebugUtilsMessengerEXT");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }


    return pf(instance, pCreateInfo, pAllocator, pCallback);
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

inline static bool CheckDeviceSuitable(const vk::PhysicalDevice& device){
    auto f = device.getFeatures(); 
    return Utils::Vulkan::QueryQueueFamilyIndices(device) && f.geometryShader? true : false;
}

void VulkanInstance::createLogicDevice(){
    float priority = 1.0;
    auto createQueueInfo = vk::DeviceQueueCreateInfo(
        vk::DeviceQueueCreateFlags(),
        _phyDeviceIndex,
        1,
        &priority
    );
    auto deviceFeat = vk::PhysicalDeviceFeatures();
    auto createInfo = vk::DeviceCreateInfo(
        vk::DeviceCreateFlags(),
        1,
        &createQueueInfo
    );
    createInfo.pEnabledFeatures = &deviceFeat;
    createInfo.enabledExtensionCount = 0;
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

    _graphicsQueue = _logicDevice->getQueue(_phyDeviceIndex, 0);
}

void VulkanInstance::SelectRunningDevice(){
    auto devices = instance.enumeratePhysicalDevices();
    if(devices.empty()){
        throw std::runtime_error("No Physical Device found");
    }

    for(auto i = 0;i < devices.size();i ++){
        //LOGI("The {}th device is {}", i, devices[i].)
        if(CheckDeviceSuitable(devices[i])){
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

void VulkanInstance::destroy(){
    if(gEnableValidationLayer){
        DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);
    }

    if(_logicDevice){
        _logicDevice->destroy();
    }
}

std::error_code VulkanInstance::initialize() {
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

    if (auto vkRet = vk::createInstance(&createInfo, nullptr, &instance); vkRet != vk::Result::eSuccess) {
        LOGE("Create Vulkan instance faield return code is {}", static_cast<int>(vkRet));
        std::runtime_error("Failed to create vulkan instance");
    }

    SelectRunningDevice();
    createLogicDevice();
    return {};
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
    if(CreateDebugUtilsMessengerEXT(instance, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo), nullptr, &callback)){
        throw std::runtime_error("Failed to register debug utils");
    }
}