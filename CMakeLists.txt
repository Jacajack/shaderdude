project(shaderdude)
cmake_minimum_required(VERSION 3.3)

add_executable(shaderdude "${PROJECT_SOURCE_DIR}/shaderdude.cpp")

find_package(glm REQUIRED)
target_link_libraries(shaderdude PRIVATE glm)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(shaderdude PRIVATE glfw)

find_package(GLEW REQUIRED)
target_link_libraries(shaderdude PRIVATE GLEW)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(shaderdude PRIVATE OpenGL::GL)
