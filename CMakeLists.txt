project(shaderdude)
cmake_minimum_required(VERSION 3.3)

include(cmake/get_imgui.cmake)

set(CMAKE_CXX_STANDARD 17)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(glm REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(shaderdude "${PROJECT_SOURCE_DIR}/shaderdude.cpp")
target_link_libraries(shaderdude PRIVATE imgui_glfw glm glfw GLEW OpenGL::GL)
