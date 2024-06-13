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

		struct VKSwapChainSupportStatus{
			vk::SurfaceCapabilitiesKHR capas{};
			std::vector<vk::SurfaceFormatKHR> formats{};
			std::vector<vk::PresentModeKHR> modes{};
		};

		std::vector<const char*> QueryGlfwExtension();

		std::vector<VkExtensionProperties> QueryVkExtensions();

		std::vector<VkLayerProperties> QueryVkValidationLayers();

		bool CheckValidationLayerSupport(const std::vector<const char*> &layers);

		VKQueueFamilyIndices QueryQueueFamilyIndices(const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface);

		bool CheckDeviceExtensionSupport(const vk::PhysicalDevice &device, std::vector<const char*> deviceExtenions);
		 
		VKSwapChainSupportStatus QuerySwapChainStatus(const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface);
	}
}