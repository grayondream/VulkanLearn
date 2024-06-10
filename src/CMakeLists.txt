



aux_source_directory(. MAIN_SRC_FILES)

message(STATUS "The source file list ${MAIN_SRC_FILES}")
set(SOURCE_FILES ${MAIN_SRC_FILES})

find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)