#include "curve_renderer.hpp"

CurveRenderer::CurveRenderer(GmsWindow &window, GmsGui &gui, std::vector<Curve> &curveData) : GmsRenderer(window, gui), curves(curveData)
{
    GmsRenderer::setVertexData(getAllGLVertexData(curves));
}

CurveRenderer::~CurveRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

const void CurveRenderer::renderCurves()
{
    // this call is prob inefficient and we might just want to add a member var for this later or only call this if the user is switching between patches and curves
    GmsRenderer::setVertexData(getAllGLVertexDataCol(curves));
    glBindVertexArray(VAO);

    // draw curves
    glUseProgram(curveShaderId);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
    for (int i = 0; i < curves.size(); i++)
        glDrawArrays(GL_PATCHES, i * 4, 4);

    // draw the lines between control point and handle
    glUseProgram(lineShaderId);
    glLineWidth(1.2f);
    for (int i = 0; i < curves.size(); i++)
        glDrawArrays(GL_LINES, i * 4, 4);

    // draw points
    glUseProgram(pointShaderId);
    GmsRenderer::highlightSelectedPoint(4);

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, curves.size() * 4);
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
    updateCurveData();
    renderCurves();
}