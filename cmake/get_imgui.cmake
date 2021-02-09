include(FetchContent)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL 3 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

FetchContent_Populate(imgui
	GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
	GIT_TAG "v1.80"
)

add_library(imgui_glfw STATIC
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_tables.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
	)
target_compile_definitions(imgui_glfw PUBLIC "-DIMGUI_IMPL_OPENGL_LOADER_GLEW")
target_link_libraries(imgui_glfw PUBLIC glfw GLEW OpenGL::GL)
target_include_directories(imgui_glfw PUBLIC
	"${imgui_SOURCE_DIR}"
	"${imgui_SOURCE_DIR}/backends"
	)
