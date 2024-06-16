#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Vertex{
    glm::vec2 pos{};
    glm::vec3 color{};

public:
    static vk::VertexInputBindingDescription getBindingDesc(){
        vk::VertexInputBindingDescription desc = {};
        desc.binding = 0;
        desc.stride = sizeof(Vertex);
        desc.inputRate = vk::VertexInputRate::eVertex;
        return desc;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDesc(){
        std::array<vk::VertexInputAttributeDescription, 2> desc = {};
        desc[0].binding = 0;
        desc[0].location = 0;
        desc[0].format = vk::Format::eR32G32Sfloat;
        desc[0].offset = offsetof(Vertex, pos);

        desc[1].binding = 0;
        desc[1].location = 1;
        desc[1].format = vk::Format::eR32G32B32Sfloat;
        desc[1].offset = offsetof(Vertex, color);
        return desc;
    }
};