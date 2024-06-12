#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <string_view>

namespace Utils {
	namespace Vulkan {
		struct VKQueueFamilyIndices{
			std::optional<uint32_t> graphics{};
			std::optional<uint32_t> present{};

			bool isComplete(){
				return graphics.has_value() && present.has_value();
			}
		};

		std::vector<const char*> QueryGlfwExtension();

		std::vector<VkExtensionProperties> QueryVkExtensions();

		std::vector<VkLayerProperties> QueryVkValidationLayers();

		bool CheckValidationLayerSupport(const std::vector<const char*> &layers);

		VKQueueFamilyIndices QueryQueueFamilyIndices(const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface);
	}
}