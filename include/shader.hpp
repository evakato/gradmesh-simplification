#pragma once

#include <glad/glad.h>

#include <array>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

struct ShaderSource
{
    const char *filepath;
    GLenum type;
};

void checkCompileErrors(unsigned int shader, std::string type);
GLuint compileShader(ShaderSource source);

template <size_t N>
GLuint linkShaders(const std::array<ShaderSource, N> &shaders)
{
    GLuint programId = glCreateProgram();
    for (auto shader : shaders)
    {
        GLuint shaderId = compileShader(shader);
        glAttachShader(programId, shaderId);
        // i hope this is allowed (deleting shader before linking)
        // glDeleteShader(shaderId);
    }

    glLinkProgram(programId);
    checkCompileErrors(programId, "PROGRAM");
    return programId;
}

constexpr std::array<ShaderSource, 4> curveShaders{
    ShaderSource{"../shaders/bezier.vs.glsl", GL_VERTEX_SHADER},
    ShaderSource{"../shaders/bezier.tcs.glsl", GL_TESS_CONTROL_SHADER},
    ShaderSource{"../shaders/bezier.tes.glsl", GL_TESS_EVALUATION_SHADER},
    ShaderSource{"../shaders/bezier.fs.glsl", GL_FRAGMENT_SHADER},
};

constexpr std::array<ShaderSource, 2> pointShaders{
    ShaderSource{"../shaders/controlpt.vs.glsl", GL_VERTEX_SHADER},
    ShaderSource{"../shaders/controlpt.fs.glsl", GL_FRAGMENT_SHADER},
};

constexpr std::array<ShaderSource, 2> lineShaders{
    ShaderSource{"../shaders/controlpt.vs.glsl", GL_VERTEX_SHADER},
    ShaderSource{"../shaders/line.fs.glsl", GL_FRAGMENT_SHADER},
};

constexpr std::array<ShaderSource, 4> patchShaders{
    ShaderSource{"../shaders/patch.vs.glsl", GL_VERTEX_SHADER},
    ShaderSource{"../shaders/patch.tcs.glsl", GL_TESS_CONTROL_SHADER},
    ShaderSource{"../shaders/patch.tes.glsl", GL_TESS_EVALUATION_SHADER},
    ShaderSource{"../shaders/patch.fs.glsl", GL_FRAGMENT_SHADER},
};