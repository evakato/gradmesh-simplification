#pragma once

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

struct ShaderSource {
    const char* filepath;
    GLenum type;
};

void checkCompileErrors(unsigned int shader, std::string type);
GLuint compileShader(ShaderSource source);
GLuint linkShaders(const std::vector<ShaderSource>& shaders);