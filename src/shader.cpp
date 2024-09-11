#include "shader.hpp"

void checkCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

GLuint compileShader(ShaderSource source) {
    std::string shaderCode;
    std::ifstream shaderFile;
    shaderFile.open(source.filepath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    shaderFile.close();
    shaderCode = shaderStream.str();
    const char* charShaderCode = shaderCode.c_str();
    GLuint shaderId = glCreateShader(source.type);
    glShaderSource(shaderId, 1, &charShaderCode, NULL);
    glCompileShader(shaderId);
    checkCompileErrors(shaderId, "VERTEX");
    return shaderId;
}

GLuint linkShaders(const std::vector<ShaderSource>& shaders)
{
    GLuint programId = glCreateProgram();
    for (auto shader : shaders) {
        GLuint shaderId = compileShader(shader);
        glAttachShader(programId, shaderId);
        // i hope this is allowed (deleting shader before linking)
        //glDeleteShader(shaderId);
    }

    glLinkProgram(programId);
    checkCompileErrors(programId, "PROGRAM");
    return programId;
}