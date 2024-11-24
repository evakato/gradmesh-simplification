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
    void highlightSelectedPoint(int numOfVerts);

    GLuint VAO, VBO, EBO;
    GLuint curveShaderId, pointShaderId, lineShaderId;

    glm::mat4 projectionMatrix;
    GmsAppState &appState;

private:
    GmsWindow &window;
};

void initializeOpenGL();
void setUniformProjectionMatrix(GLuint shaderId, glm::mat4 &projectionMatrix);
void setLineColor(GLuint shaderId, const glm::vec3 &color);
void setVertexData(const std::vector<GLfloat> vertexData);
void setProjectionMatrixAABB(int shaderId, AABB aabb);
void drawPrimitive(const std::vector<GLfloat> &glData, int shaderId, glm::mat4 &projectionMatrix, int vertsPerPrimitive);
void drawPrimitive(const std::vector<GLfloat> &glData, int shaderId, const AABB &aabb, int vertsPerPrimitive);