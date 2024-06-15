#version 450

layout(location = 0) in vec3 inPosition;  // 输入顶点位置

void main() {
    gl_Position = vec4(inPosition, 1.0);  // 将输入位置直接传递给gl_Position
}
