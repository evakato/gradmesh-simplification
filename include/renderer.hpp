#pragma once

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.hpp"
#include "window.hpp"

class GmsAppState;

class GmsRenderer
{
public:
    GmsRenderer(GmsWindow &window, GmsAppState &appState);
    ~GmsRenderer();
    void render();

protected:
    void setProjectionMatrix();
    void bindBuffers();
    void setVertexData(std::vector<GLfloat> newData);
    void highlightSelectedPoint(int numOfVerts);

    GLuint VAO, VBO, EBO;
    GLuint curveShaderId, pointShaderId, lineShaderId;

    glm::mat4 projectionMatrix;
    std::vector<GLfloat> vertexData;
    GmsAppState &appState;

private:
    GmsWindow &window;
};

void initializeOpenGL();
void setUniformProjectionMatrix(GLuint shaderId, glm::mat4 &projectionMatrix);