#include "Utils.h"
#include "Log.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <format>
#include <algorithm>

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

		void RemoveVkUnSupportExtensions(std::vector<const char*>& ls) {
			auto vkExts = QueryVkValidationLayers();
			for (auto it = ls.begin(); it != ls.end(); ){
				auto name = *it;
				auto isSame = [&name](const VkLayerProperties& p) {
					return p.layerName == std::string_view(name);
				};
				if (std::end(vkExts) == std::find_if(vkExts.begin(), vkExts.end(), isSame)) {
					it = ls.erase(it);
				} else {
					it++;
				}
			}
		}

		std::vector<const char*> QuerySupportedGlfwExtension() {
			auto exts = QueryGlfwExtension();
			RemoveVkUnSupportExtensions(exts);
			return exts;
		}
	}
}