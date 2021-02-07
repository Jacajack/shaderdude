project(shaderdude)
cmake_minimum_required(VERSION 3.3)

set(CMAKE_CXX_STANDARD 17)

# Fetch Imgui as git submodule
find_package(Git REQUIRED)
message(STATUS "Updating submodules...")
execute_process(
	COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	RESULT_VARIABLE GIT_SUBMOD_RESULT
	)

if (NOT GIT_SUBMOD_RESULT EQUAL "0")
	message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}")
endif()

add_executable(shaderdude 
	"${PROJECT_SOURCE_DIR}/shaderdude.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/imgui.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/imgui_draw.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/imgui_tables.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
	"${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
	)

target_include_directories(shaderdude PRIVATE "${PROJECT_SOURCE_DIR}/imgui" "${PROJECT_SOURCE_DIR}/imgui/backends")
target_compile_definitions(shaderdude PRIVATE "-DIMGUI_IMPL_OPENGL_LOADER_GLEW")
	
find_package(glm REQUIRED)
target_link_libraries(shaderdude PRIVATE glm)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(shaderdude PRIVATE glfw)

find_package(GLEW REQUIRED)
target_link_libraries(shaderdude PRIVATE GLEW)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(shaderdude PRIVATE OpenGL::GL)
