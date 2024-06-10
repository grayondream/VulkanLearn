#pragma once
#include <memory>
#include <system_error>

struct GLFWwindow;

class VulkanInstance;
class Application {
public:
	std::error_code init();
	std::error_code run();
	std::error_code destroy();

private:
	std::error_code initWindow();

private:
	GLFWwindow* pwin{};
	std::shared_ptr<VulkanInstance> instance{};
};