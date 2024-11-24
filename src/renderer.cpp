#include <glm/gtc/type_ptr.hpp>

#include "renderer.hpp"
#include "gms_app.hpp"

GmsRenderer::GmsRenderer(GmsWindow &window, GmsAppState &appState) : window{window}, appState{appState}
{
    initializeOpenGL();
    bindBuffers();
}

void GmsRenderer::bindBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    curveShaderId = linkShaders(curveShaders);
    appState.patchRenderResources.curveShaderId = curveShaderId;
    pointShaderId = linkShaders(pointShaders);
    lineShaderId = linkShaders(lineShaders);

    glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &appState.maxHWTessellation);
}

GmsRenderer::~GmsRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void GmsRenderer::render()
{
    glViewport(0, 0, GL_LENGTH, GL_LENGTH);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    window.processInput();
    setProjectionMatrix();
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glPolygonMode(GL_FRONT_AND_BACK, appState.isWireframeMode ? GL_LINE : GL_FILL);
}

void GmsRenderer::setProjectionMatrix()
{
    projectionMatrix = glm::mat4(1.0f);
    float height = std::exp(GmsWindow::zoom);
    float width = 1.0f * height;
    float left = -width * 0.5f + GmsWindow::viewPos.x;
    float right = width * 0.5f + GmsWindow::viewPos.x;
    float bottom = height * 0.5f + GmsWindow::viewPos.y;
    float top = -height * 0.5f + GmsWindow::viewPos.y;
    projectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}

void GmsRenderer::highlightSelectedPoint(int numOfVerts)
{
    if (GmsWindow::validSelectedPoint())
    {
        int selectedIndex = GmsWindow::selectedPoint.primitiveId * numOfVerts + GmsWindow::selectedPoint.pointId;
        glUniform1i(glGetUniformLocation(pointShaderId, "selectedIndex"), selectedIndex);
    }
    else
    {
        glUniform1i(glGetUniformLocation(pointShaderId, "selectedIndex"), -1);
    }
}

void initializeOpenGL()
{
    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void setLineColor(GLuint shaderId, const glm::vec3 &color)
{
    GLint lineColorLocation = glGetUniformLocation(shaderId, "lineColor");
    glUniform3fv(lineColorLocation, 1, glm::value_ptr(color));
}

void setVertexData(std::vector<GLfloat> vertexData)
{
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(GLfloat), vertexData.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);
}

void setUniformProjectionMatrix(GLuint shaderId, glm::mat4 &projectionMatrix)
{
    GLint projectionLoc = glGetUniformLocation(shaderId, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projectionMatrix[0][0]);
}

void setProjectionMatrixAABB(int shaderId, AABB aabb)
{
    glm::mat4 projMat = glm::mat4(1.0f);
    projMat = glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, -1.0f, 1.0f);
    setUniformProjectionMatrix(shaderId, projMat);
}

void drawPrimitive(const std::vector<GLfloat> &glData, int shaderId, glm::mat4 &projectionMatrix, int vertsPerPrimitive)
{
    setVertexData(glData);
    glUseProgram(shaderId);
    setUniformProjectionMatrix(shaderId, projectionMatrix);
    glPatchParameteri(GL_PATCH_VERTICES, vertsPerPrimitive);
    glDrawArrays(GL_PATCHES, 0, glData.size() / 5);
}

void drawPrimitive(const std::vector<GLfloat> &glData, int shaderId, const AABB &aabb, int vertsPerPrimitive)
{
    setVertexData(glData);
    glUseProgram(shaderId);
    setProjectionMatrixAABB(shaderId, aabb);
    glPatchParameteri(GL_PATCH_VERTICES, vertsPerPrimitive);
    glDrawArrays(GL_PATCHES, 0, glData.size() / 5);
}