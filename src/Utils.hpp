#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <string_view>

namespace Utils {
	namespace Vulkan {
		std::vector<const char*> QueryGlfwExtension();

		std::vector<VkExtensionProperties> QueryVkExtensions();

		std::vector<VkLayerProperties> QueryVkValidationLayers();

		bool CheckValidationLayerSupport(const std::vector<const char*> &layers);
	}
}