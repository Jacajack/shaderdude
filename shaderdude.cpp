#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std::string_literals;

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
		throw std::runtime_error("Shader compilation failed:\n"s + log + "\n"s);
	
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

GLuint load_fragment_shader(const std::string &path)
{
	static const std::string prefix = 
	"#version 430 core\n"
	
	"in VS_OUT"
	"{"
	"	vec2 uv;"
	"} vs_out;"
	
	"uniform float iTime;"
	"uniform vec3 iResolution;"
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
	
	return create_shader(GL_FRAGMENT_SHADER, prefix + slurp_txt(path) + suffix);
}

GLuint make_program(const std::string &path)
{
	GLuint prog = glCreateProgram();
	GLuint vsh = create_vertex_shader();
	GLuint fsh = load_fragment_shader(path);
	glAttachShader(prog, vsh);
	glAttachShader(prog, fsh);
	glLinkProgram(prog);
	glDeleteShader(vsh);
	glDeleteShader(fsh);
	return prog;
}

time_t get_mod_time(const std::string &path)
{
	struct stat result;
	if (stat(path.c_str(), &result) == 0)
		return result.st_mtime;
	else
		throw std::runtime_error("could not get modification time");
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;
	
	const std::string shader_path = argv[1];
	
	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_CORE_PROFILE, GL_TRUE);
	
	GLFWwindow *win = glfwCreateWindow(720, 480, ("shaderdude - "s + shader_path).c_str(), NULL, NULL);
	if (win == nullptr) throw std::runtime_error("glfwCreateWindow() failed");
	
	glfwMakeContextCurrent(win);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) throw std::runtime_error("glewInit() failed");
	
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
	
	glDisable(GL_DEPTH_TEST);
	
	long frame_counter = 0;
	while (!glfwWindowShouldClose(win))
	{
		double t = glfwGetTime();
		static int win_w = 0, win_h = 0;
		static GLuint program = 0;
		static time_t shader_mod_time = 0;
		
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
						glDeleteProgram(program);
						program = 0;
						program = make_program(shader_path);
						glUseProgram(program);
					}
					catch (const std::exception &ex)
					{
						std::cerr << "Loading shader failed!" << std::endl;
						std::cerr << ex.what() << std::endl;
					}
					glfwSetTime(0.0);
				}
			}
			catch (const std::exception &ex)
			{
				// Ignore file access errors
			}
		}
		
		glClear(GL_COLOR_BUFFER_BIT);
		glUniform1f(glGetUniformLocation(program, "iTime"), t);
		glUniform3f(glGetUniformLocation(program, "iResolution"), win_w, win_h, 0);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glfwPollEvents();
		glfwSwapBuffers(win);
		frame_counter++;
	}
	
	return 0;
}
