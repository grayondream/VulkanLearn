#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <string_view>

namespace Utils {
	namespace Vulkan {
		std::vector<const char*> QueryGlfwExtension();

		std::vector<VkExtensionProperties> QueryVkExtensions();

		std::vector<VkLayerProperties> QueryVkValidationLayers();

		bool CheckValidationLayerSupport(const std::vector<const char*> &layers);

		std::optional<int32_t>  QueryQueueFamilyIndices(const vk::PhysicalDevice &dev);
	}
}