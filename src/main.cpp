
#include <iostream>
#include <format>


#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include "Application.hpp"
#include "Log.hpp"


int main(int argc, char **argv){
    LOGI("Hello Vulkan");
    Application app{};
    if (app.init()) {
        LOGE("initialize the application failed");
        exit(1);
    }

    if (app.run()) {
        LOGE("can not run the application normally");
    }

    app.destroy();
    LOGI("Good Bye Vulkan");
    return 0;
}