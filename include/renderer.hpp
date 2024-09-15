#pragma once

#include "gui.hpp"
#include "shader.hpp"
#include "window.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

class GmsRenderer
{
public:
    GmsRenderer(GmsWindow &window, GmsGui &gui);
    ~GmsRenderer();

    const void render();

protected:
    void setProjectionMatrix();
    const void setupShaders();
    const void startRenderFrame();
    const void setVertexData(std::vector<GLfloat> newData);
    const void highlightSelectedPoint(int numOfVerts);

    GLuint VAO, VBO, EBO;
    GLuint curveShaderId, pointShaderId, lineShaderId;

    glm::mat4 projectionMatrix;

    std::vector<GLfloat> vertexData;
    GmsGui &gui;

private:
    GmsWindow &window;
};

void initializeOpenGL();
void setUniformProjectionMatrix(GLuint shaderId, glm::mat4 &projectionMatrix);