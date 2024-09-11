#pragma once

#include "gui.hpp"
#include "shader.hpp"
#include "window.hpp"

#include <glad/glad.h>

#include <vector>

class GmsRenderer
{
public:
    GmsRenderer(GmsWindow &window, GmsGui &gui);
    ~GmsRenderer();

    const void render();

protected:
    const void setupShaders();
    const void startRenderFrame();
    const void setVertexData(std::vector<GLfloat> newData);
    const void highlightSelectedPoint(int numOfVerts);

    GLuint VAO, VBO, EBO;
    GLuint curveShaderId, pointShaderId, lineShaderId;

    std::vector<GLfloat> vertexData;
    GmsGui &gui;

private:
    GmsWindow &window;
};

const void initializeOpenGL();