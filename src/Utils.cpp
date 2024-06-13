#include "Utils.hpp"
#include "Log.hpp"
#include <GLFW/glfw3.h>
#include <climits>
#include <cstdint>
#include <iostream>
#include <format>
#include <algorithm>
#include <optional>
#include <utility>
#include <vulkan/vulkan_core.h>
#include <string>
#include <set>

namespace Utils {
namespace Vulkan {
std::vector<const char*> QueryGlfwExtension() {
	uint32_t cnt{};
	const char** exts = glfwGetRequiredInstanceExtensions(&cnt);
	std::vector<const char*> extVec{ exts, exts + cnt };
#ifdef NDEBUG
	extVec.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif//NDEBUG
	return extVec;
}

std::vector<VkExtensionProperties> QueryVkExtensions() {
	unsigned int cnt{};
	if (auto ret = vkEnumerateInstanceExtensionProperties(nullptr, &cnt, nullptr); ret != VK_SUCCESS) {
		LOGE("Can not get the count of extensions. Vk Error Code is {}", static_cast<int>(ret));
		return {};
	}

	std::vector<VkExtensionProperties> exts(cnt);

	if (auto ret = vkEnumerateInstanceExtensionProperties(nullptr, &cnt, exts.data()); ret != VK_SUCCESS) {
		LOGE("The count number is {}, Can not get the count of extensions. Vk Error Code is {}", cnt, static_cast<int>(ret));
		std::vector<VkExtensionProperties>().swap(exts);
		return {};
	}

	return exts;
}

std::vector<VkLayerProperties> QueryVkValidationLayers() {
	uint32_t lyCnt{};
	if (auto ret = vkEnumerateInstanceLayerProperties(&lyCnt, nullptr)) {
		LOGE("Can not get Vulkan Layer count {}.Vulkan errorcode is {}", lyCnt, static_cast<int>(ret));
		return {};
	}

	std::vector<VkLayerProperties> props{lyCnt};
	if (auto ret = vkEnumerateInstanceLayerProperties(&lyCnt, props.data())) {
		LOGE("Can not get Vulkan Layer count {}.Vulkan errorcode is {}", lyCnt, static_cast<int>(ret));
		std::vector<VkLayerProperties>().swap(props);
		return {};
	}
	return props;
}

bool CheckValidationLayerSupport(const std::vector<const char*> &layers) {
	const auto spLayers = QueryVkValidationLayers();
	for (auto&& lname : layers) {
		for (auto&& spl : spLayers) {
			if (std::string_view(spl.layerName) == std::string_view(lname)) {
				return true;
			}
		}
	}

	return false;
}

VKQueueFamilyIndices QueryQueueFamilyIndices(const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface){
	VKQueueFamilyIndices indices{};
	auto qfPro = dev.getQueueFamilyProperties();
	for(auto i = 0;i < qfPro.size();i ++){
		auto q = qfPro[i];
		if(q.queueCount > 0 && q.queueFlags & vk::QueueFlagBits::eGraphics){
			indices.graphics = i;
		}

		if(q.queueCount > 0 && dev.getSurfaceSupportKHR(i, surface)){
			indices.present = i;
		}

		if(indices.isComplete()){
			break;
		}
	}

	return indices;
}

bool CheckDeviceExtensionSupport(const vk::PhysicalDevice &device, std::vector<const char*> deviceExtenions){
	std::set<std::string> exts(deviceExtenions.begin(), deviceExtenions.end());
	for(auto && e : device.enumerateDeviceExtensionProperties()){
		exts.erase(e.extensionName);
	}

	return exts.empty();
}

VKSwapChainSupportStatus QuerySwapChainStatus(const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface){
	VKSwapChainSupportStatus status{};
	status.capas = device.getSurfaceCapabilitiesKHR(surface);
	status.formats = device.getSurfaceFormatsKHR(surface);
	status.modes = device.getSurfacePresentModesKHR(surface);
	return status;
}
}
}