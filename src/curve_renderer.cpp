#include "curve_renderer.hpp"

CurveRenderer::CurveRenderer(GmsWindow &window, GmsAppState &appState) : GmsRenderer(window, appState)
{
    glm::vec3 col1{0.0f, 1.0f, 0.0f};
    glm::vec3 col2{1.0f, 1.0f, 0.0f};
    glm::vec3 col3{0.5f, 0.2f, 0.7f};
    glm::vec3 col4{1.0f, 0.2f, 0.2f};

    Curve curve3{
        {glm::vec2(-0.2f, -0.2f), col1},
        {glm::vec2(-0.4f, 0.45f), col2},
        {glm::vec2(0.4f, 0.3f), col4},
        {glm::vec2(0.45f, 0.0f), col3},
    };

    curves = {curve3};
    GmsRenderer::setVertexData(getAllGLVertexDataCol(curves));
}

CurveRenderer::~CurveRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

const void CurveRenderer::renderCurves()
{
    auto curveData = getAllGLVertexDataCol(curves);
    // this call is prob inefficient and we might just want to add a member var for this later or only call this if the user is switching between patches and curves
    GmsRenderer::setVertexData(curveData);

    // draw curves
    glUseProgram(curveShaderId);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
    glLineWidth(appState.curveLineWidth);
    setUniformProjectionMatrix(curveShaderId, projectionMatrix);
    glDrawArrays(GL_PATCHES, 0, curveData.size() / 5);

    // draw the lines between control point and handle
    glUseProgram(lineShaderId);
    glLineWidth(appState.handleLineWidth);
    setUniformProjectionMatrix(lineShaderId, projectionMatrix);
    glDrawArrays(GL_LINES, 0, curveData.size() / 5);

    // draw points
    glUseProgram(pointShaderId);
    setUniformProjectionMatrix(pointShaderId, projectionMatrix);
    // GmsRenderer::highlightSelectedPoint(4);
    glDrawArrays(GL_POINTS, 0, curveData.size() / 5);
}

const void CurveRenderer::updateCurveData()
{

    if (GmsWindow::isClicked)
    {
        GmsWindow::selectedPoint = getSelectedPointId(curves, GmsWindow::mousePos);
        // gmsgui::setCurrentColor(getColorAtPoint(curves, GmsWindow::selectedPoint));
        GmsWindow::isClicked = false;
    }
    /*
    else if (GmsWindow::validSelectedPoint() && gmsgui::hasColorChanged)
    {
        // yeah im not doing this rn
        //std::cout << "color changed" << std::endl;
        //gmsgui::resetColorChange();
    }
    */
    else if (GmsWindow::isDragging && GmsWindow::validSelectedPoint())
    {
        setCurveCoords(curves, GmsWindow::selectedPoint, GmsWindow::mousePos);
        size_t offset = GmsWindow::selectedPoint.primitiveId * 4 * sizeof(glm::vec2) + (GmsWindow::selectedPoint.pointId * sizeof(glm::vec2));
        glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec2), &GmsWindow::mousePos);
    }
}

const void CurveRenderer::render()
{
    GmsRenderer::render();
    // updateCurveData();
    renderCurves();
}