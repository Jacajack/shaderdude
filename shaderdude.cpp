#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

using namespace std::string_literals;

struct texture 
{
	std::string filename;
	GLuint tex;
	uint8_t *data;
	int width;
	int height;
	int channels;
	
	texture(const texture&) = delete;
	texture &operator=(const texture&) = delete;
	
	texture(texture &&src) :
		filename(std::move(src.filename)),
		tex(src.tex),
		data(src.data),
		width(src.width),
		height(src.height),
		channels(src.channels)
	{
		src.tex = 0;
		src.data = nullptr;
	}
	
	explicit texture(const std::string &path)
	{
		data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		if (!data)
			throw std::runtime_error("failed to load image '"s + path + "'"s);
		
		// Reorder
		int row_size = width * channels;
		for (int y = 0; y < height / 2; y++)
			std::swap_ranges(data + y * row_size, data + (y + 1) * row_size, data + (height - 1 - y) * row_size);
		
		GLenum data_format;
		if (channels == 1) data_format = GL_RED;
		else if (channels == 2) data_format = GL_RG;
		else if (channels == 3) data_format = GL_RGB;
		else data_format = GL_RGBA;
		
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
		glTextureSubImage2D(tex, 0, 0, 0, width, height, data_format, GL_UNSIGNED_BYTE, data);  
	}
	
	~texture()
	{
		if (tex) glDeleteTextures(1, &tex);
		if (data) stbi_image_free(data);
	}
};

struct shader_uniform
{
	std::string name;
	GLint location;
	GLenum type;
	
	shader_uniform() :
		location(-1)
	{}
	
	shader_uniform(const std::string &n, GLint l, GLenum t) :
		name(n),
		location(l),
		type(t)
	{}
};

struct shader_program
{
	GLuint id;
	std::map<std::string, shader_uniform> uniforms;
	
	shader_program(const shader_program &) = delete;
	shader_program &operator=(const shader_program &) = delete;
	
	explicit shader_program(GLuint i) :
		id(i)
	{
		GLint count;
		glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);
		
		for (int loc = 0; loc < count; loc++)
		{
			char buf[256];
			GLsizei length;
			GLint size;
			GLenum type;
			glGetActiveUniform(id, loc, sizeof(buf), &length, &size, &type, buf);
			uniforms[buf] = shader_uniform(buf, loc, type);
		}
	}
	
	~shader_program()
	{
		glDeleteProgram(id);
	}
};

bool gui_visible = true;

std::string slurp_txt(const std::string &path)
{
	std::ifstream f(path);
	if (!f) throw std::runtime_error("could not read file '"s + path + "'"s);
	std::stringstream buf;
	buf << f.rdbuf();
	return buf.str();
}

GLuint get_shader_log(GLuint id, std::string &log)
{
	GLint result, length;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

	if ( length > 0 )
	{
		char *buf = new char[length + 1];
		glGetShaderInfoLog(id, length, NULL, buf);
		log = std::string(buf);
		delete[] buf;
	}
	else
	{
		log = "";
	}

	return result;
}

GLuint create_shader(GLenum type, const std::string &source)
{
	GLuint shader = glCreateShader(type);
	char *buf = new char[source.length() + 1];
	std::strncpy(buf, source.c_str(), source.length() + 1);
	glShaderSource(shader, 1, &buf, NULL);
	glCompileShader(shader);
	delete[] buf;
	
	std::string log;
	if (get_shader_log(shader, log) == GL_FALSE)
	{
		glDeleteShader(shader);
		throw std::runtime_error("Shader compilation failed:\n"s + log + "\n"s);
	}
	
	return shader;
}

GLuint create_vertex_shader()
{
	static const std::string source = 
	"#version 430 core\n"

	"out VS_OUT"
	"{"
	"	vec2 uv;"
	"} vs_out;"

	"const vec2 vertices[6] = vec2[6]("
	"	vec2(-1, -1),"
	"	vec2(1, -1),"
	"	vec2(-1, 1),"
	"	vec2(-1, 1),"
	"	vec2(1, -1),"
	"	vec2(1, 1)"
	");"

	"void main()"
	"{"
	"	vec2 vertex = vertices[gl_VertexID];"
	"	gl_Position = vec4(vertex.xy, 0, 1);"
	"	vs_out.uv = vertex * 0.5 + 0.5;"
	"}";
	
	return create_shader(GL_VERTEX_SHADER, source);
}

GLuint load_fragment_shader(const std::string &path, int texture_count)
{
	static const std::string prefix = 
	"#version 430 core\n"
	
	"in VS_OUT"
	"{"
	"	vec2 uv;"
	"} vs_out;"
	
	"uniform float iTime;"
	"uniform vec3 iResolution;"
	"uniform vec4 iMouse;"
	"uniform int iFrame;"
	"out vec4 f_color;"
	"\n";
	
	static const std::string suffix = 
	"\n"
	"void main()"
	"{"
	"	vec2 fragCoord = vs_out.uv * iResolution.xy;"
	"	vec4 fragColor;"
	"	mainImage(fragColor, fragCoord);"
	"	f_color = fragColor;"
	"}"
	"\n";
	
	std::stringstream texture_bindings;
	for (int i = 0; i < texture_count; i++)
		texture_bindings << "layout (binding = " << i << ") uniform sampler2D iChannel" << i << ";\n";
	texture_bindings << "uniform vec3 iChannelResolution[" << texture_count << "];\n";
	
	std::string shader_source = prefix + texture_bindings.str() + slurp_txt(path) + suffix;
	return create_shader(GL_FRAGMENT_SHADER, shader_source);
}

std::unique_ptr<shader_program> make_program(const std::string &path, int texture_count)
{
	GLuint vsh = 0, fsh = 0;
	
	try
	{
		vsh = create_vertex_shader();
		fsh = load_fragment_shader(path, texture_count);
	}
	catch (...)
	{
		glDeleteShader(vsh);
		glDeleteShader(fsh);
		std::rethrow_exception(std::current_exception());
	}
	
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vsh);
	glAttachShader(prog, fsh);
	glLinkProgram(prog);
	glDeleteShader(vsh);
	glDeleteShader(fsh);
	
	return std::make_unique<shader_program>(prog);
}

time_t get_mod_time(const std::string &path)
{
	struct stat result;
	if (stat(path.c_str(), &result) == 0)
		return result.st_mtime;
	else
		throw std::runtime_error("could not get modification time");
}

void glfw_error_callback(int error, const char *message)
{
	throw std::runtime_error("GLFW error - "s + message);
}

void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
		gui_visible = !gui_visible;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;
	
	const std::string shader_path = argv[1];
	
	glfwSetErrorCallback(glfw_error_callback);
	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	GLFWwindow *win = glfwCreateWindow(720, 480, ("shaderdude - "s + shader_path).c_str(), NULL, NULL);
	if (win == nullptr) throw std::runtime_error("glfwCreateWindow() failed");
	
	glfwMakeContextCurrent(win);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) throw std::runtime_error("glewInit() failed");
	
	// GLFW callbacks - these must be set up before ImGui takes control
	glfwSetKeyCallback(win, glfw_key_callback);
	
	// Imgui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &imgui_io = ImGui::GetIO(); (void)imgui_io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(win, true);
	ImGui_ImplOpenGL3_Init();
	
	// Load textures
	std::vector<texture> textures;
	for (int i = 0; i < argc - 2; i++)
	{
		try
		{
			textures.emplace_back(std::string(argv[2 + i]));
		}
		catch (const std::exception &ex)
		{
			std::cerr << "Loading textures failed: " << ex.what() << std::endl;
			return 1;
		}
	}
	
	// Check shader file
	try
	{
		get_mod_time(shader_path);
	}
	catch (const std::exception &ex)
	{
		std::cerr << "Could not open shader file!" << std::endl;
		return 1;
	}
	
	// VAO
	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	// Bind textures to samplers
	for (int i = 0; i < textures.size(); i++)
		glBindTextureUnit(i, textures[i].tex);
	
	glDisable(GL_DEPTH_TEST);
	
	// The shader
	std::unique_ptr<shader_program> program;
	
	// Some state
	int win_w = 0, win_h = 0;
	time_t shader_mod_time = 0;
	double shader_start_time = 0.0;
	long frame_counter = 0;
	std::map<std::string, int> int_uniforms_state;
	std::map<std::string, float> float_uniforms_state;
	std::map<std::string, bool> bool_uniforms_state;
	std::map<std::string, glm::vec3> vec3_uniforms_state;
	std::map<std::string, glm::vec4> vec4_uniforms_state;
	
	while (!glfwWindowShouldClose(win))
	{
		// Time
		double t = glfwGetTime();
		
		// Mouse
		double mx, my;
		bool mlb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		bool mrb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		glfwGetCursorPos(win, &mx, &my);
		
		// Poll events
		glfwPollEvents();
		
		// Imgui new frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		
		// GUI
		if (gui_visible)
		{
			ImGui::Begin("Controls");
			ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Dummy(ImVec2(0.0f, 5.0f));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, 5.0f));
			ImGui::TextWrapped("This section allows you to control uniforms with names beginning with 'ctl_'");
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			
			if (program)
				for (const auto &[name, unif] : program->uniforms)
				{
					if (name.find("ctl_") != 0) continue;
					std::string ctl_name = name.substr(4);
					
					switch (unif.type)
					{
						case GL_INT:
							ImGui::InputInt(ctl_name.c_str(), &int_uniforms_state[name]);
							break;
						
						case GL_FLOAT:
							ImGui::SliderFloat(ctl_name.c_str(), &float_uniforms_state[name], 0.0f, 1.0f);
							break;
							
						case GL_BOOL:
							ImGui::Checkbox(ctl_name.c_str(), &bool_uniforms_state[name]);
							break;
							
						case GL_FLOAT_VEC3:
							ImGui::ColorEdit3(ctl_name.c_str(), &vec3_uniforms_state[name][0]);
							break;
							
						case GL_FLOAT_VEC4:
							ImGui::ColorEdit4(ctl_name.c_str(), &vec4_uniforms_state[name][0]);
							break;
					}
				}
			
			ImGui::End();
		}
		
		// Resolution
		static int win_w = 0, win_h = 0;
		static time_t shader_mod_time = 0;
		static double shader_start_time = 0.0;
		
		if (frame_counter % 5 == 0)
		{
			int new_win_w, new_win_h;
			glfwGetWindowSize(win, &new_win_w, &new_win_h);
			if (new_win_h != win_h || new_win_w != win_w)
			{
				win_w = new_win_w;
				win_h = new_win_h;
				glViewport(0, 0, win_w, win_h);
			}
		
			try
			{
				int new_shader_mod_time = get_mod_time(shader_path);
				if (new_shader_mod_time != shader_mod_time)
				{
					try
					{
						shader_mod_time = new_shader_mod_time;
						program = make_program(shader_path, textures.size());
						glUseProgram(program->id);
						
						// std::cerr << "Successfully loaded the new shader!" << std::endl;
						// for (const auto &[k, v] : program->uniforms)
						// 	std::cerr << "\t- " << k << std::endl;
					}
					catch (const std::exception &ex)
					{
						std::cerr << "Loading shader failed!" << std::endl;
						std::cerr << ex.what() << std::endl;
					}
					
					shader_start_time = glfwGetTime();
				}
			}
			catch (const std::exception &ex)
			{
				// Ignore file access errors
			}
		}
		
		glClear(GL_COLOR_BUFFER_BIT);
		
		// Draw shader
		if (program)
		{
			glUniform1f(program->uniforms["iTime"].location, t - shader_start_time);
			glUniform3f(program->uniforms["iResolution"].location, win_w, win_h, 0);
			glUniform4f(program->uniforms["iMouse"].location, mx, my, mlb, mrb);
			glUniform1i(program->uniforms["iFrame"].location, frame_counter);
			
			for (const auto &[name, unif] : program->uniforms)
			{
				switch (unif.type)
				{
					case GL_INT:
						if (auto it = int_uniforms_state.find(name); it != int_uniforms_state.end())
							glUniform1i(unif.location, it->second);
						break;
						
					case GL_FLOAT:
						if (auto it = float_uniforms_state.find(name); it != float_uniforms_state.end())
							glUniform1f(unif.location, it->second);
						break;
					
					case GL_BOOL:
						if (auto it = bool_uniforms_state.find(name); it != bool_uniforms_state.end())
							glUniform1i(unif.location, it->second);
						break;
					
					case GL_FLOAT_VEC3:
						if (auto it = vec3_uniforms_state.find(name); it != vec3_uniforms_state.end())
							glUniform3fv(unif.location, 1, &it->second[0]);
						break;
						
					case GL_FLOAT_VEC4:
						if (auto it = vec4_uniforms_state.find(name); it != vec4_uniforms_state.end())
							glUniform4fv(unif.location, 1, &it->second[0]);
						break;
				}
			}
			
			for (int i = 0; i < textures.size(); i++)
				glUniform3f(program->uniforms["iChannelResolution["s + std::to_string(i) +"]"s].location, textures[i].width, textures[i].height, 0.f);
			
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		
		// Draw GUI
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		glfwSwapBuffers(win);
		frame_counter++;
	}
	
	// ImGui cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	
	// Cleanup
	textures.clear();
	program.reset();
	glDeleteVertexArrays(1, &vao);
	glfwTerminate();
	
	return 0;
}
