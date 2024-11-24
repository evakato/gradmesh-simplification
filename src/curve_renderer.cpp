#include "curve_renderer.hpp"
#include "gms_appstate.hpp"

CurveRenderer::CurveRenderer(GmsWindow &window, GmsAppState &appState) : GmsRenderer(window, appState) {}

CurveRenderer::~CurveRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void CurveRenderer::renderCurves()
{
    auto params = appState.curveRenderParams;
    auto curveData = getAllCurveGLData(params.curves, &Curve::getGLVertexDataCol);

    // draw curves
    glUniform1i(glGetUniformLocation(curveShaderId, "curve_type"), params.curveMode);
    glLineWidth(appState.curveLineWidth);
    drawPrimitive(curveData, curveShaderId, projectionMatrix, VERTS_PER_CURVE);

    // draw the lines between control point and handle
    auto curvePointData = getAllCurveGLData(params.curves, &Curve::getGLPointData);
    setVertexData(curvePointData);
    glUseProgram(lineShaderId);
    setLineColor(lineShaderId, black);
    glLineWidth(appState.handleLineWidth);
    setUniformProjectionMatrix(lineShaderId, projectionMatrix);
    glDrawArrays(GL_LINES, 0, curvePointData.size() / 5);

    // draw points
    glUseProgram(pointShaderId);
    setUniformProjectionMatrix(pointShaderId, projectionMatrix);
    // GmsRenderer::highlightSelectedPoint(4);
    glDrawArrays(GL_POINTS, 0, curvePointData.size() / 5);

    if (appState.showCurveAABB)
    {
        auto aabbData = getAllCurveGLData(params.curves, &Curve::getGLAABBData);
        setVertexData(aabbData);
        glUseProgram(lineShaderId);
        setLineColor(lineShaderId, blue);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        for (int i = 0; i < aabbData.size() / 5; i++)
            glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }
}

void CurveRenderer::updateCurveData()
{

    if (GmsWindow::isClicked)
    {
        GmsWindow::selectedPoint = getSelectedPointId(appState.curveRenderParams.curves, GmsWindow::mousePos);
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
        setCurveCoords(appState.curveRenderParams.curves, GmsWindow::selectedPoint, GmsWindow::mousePos);
        size_t offset = GmsWindow::selectedPoint.primitiveId * 4 * sizeof(glm::vec2) + (GmsWindow::selectedPoint.pointId * sizeof(glm::vec2));
        glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec2), &GmsWindow::mousePos);
    }
}

void CurveRenderer::render()
{
    GmsRenderer::render();
    // updateCurveData();
    renderCurves();
}