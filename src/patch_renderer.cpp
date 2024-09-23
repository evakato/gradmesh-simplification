#include "patch_renderer.hpp"
#include "gms_app.hpp"

PatchRenderer::PatchRenderer(GmsWindow &window, GmsAppState &appState) : GmsRenderer(window, appState)
{
}

void PatchRenderer::bindBuffers()
{
    GmsRenderer::bindBuffers();
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    patchShaderId = linkShaders(patchShaders);
    // GmsRenderer::setVertexData(getAllPatchData(patches, &Patch::getControlMatrix));
}

PatchRenderer::~PatchRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void PatchRenderer::renderPatches(std::vector<Patch> &patches)
{
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // glPolygonMode(GL_FRONT_AND_BACK, gui.isWireframeMode ? GL_LINE : GL_FILL);

    if (appState.renderPatches)
    {
        // same with this call
        GmsRenderer::setVertexData(getAllPatchData(patches, &Patch::getControlMatrix));
        glUseProgram(patchShaderId);
        setUniformProjectionMatrix(patchShaderId, projectionMatrix);
        glLineWidth(1.0f);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
        for (int i = 0; i < patches.size(); i++)
            glDrawArrays(GL_PATCHES, i * VERTS_PER_PATCH, VERTS_PER_PATCH);
    }

    if (appState.renderCurves)
    {
        GmsRenderer::setVertexData(getAllPatchData(patches, &Patch::getCurveData));
        glUseProgram(curveShaderId);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
        glLineWidth(appState.curveLineWidth);
        setUniformProjectionMatrix(curveShaderId, projectionMatrix);
        glDrawArrays(GL_PATCHES, 0, 16 * patches.size());
    }

    if (appState.renderHandles)
    {
        const std::vector<GLfloat> pointHandleData = getAllPatchData(patches, &Patch::getPointHandleData);
        GmsRenderer::setVertexData(pointHandleData);
        glUseProgram(lineShaderId);
        glLineWidth(appState.handleLineWidth);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        glDrawArrays(GL_LINES, 0, pointHandleData.size() / 2);

        const std::vector<GLfloat> allPatchHandleData = getAllPatchData(patches, &Patch::getHandleData);
        GmsRenderer::setVertexData(allPatchHandleData);
        glUseProgram(pointShaderId);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);
        // GmsRenderer::highlightSelectedPoint(12);
        glDrawArrays(GL_POINTS, 0, allPatchHandleData.size() / 5);
    }

    if (appState.renderControlPoints)
    {
        const std::vector<GLfloat> allPatchPointData = getAllPatchData(patches, &Patch::getPointData);
        GmsRenderer::setVertexData(allPatchPointData);
        glUseProgram(pointShaderId);
        // const std::vector<Vertex> allPointData = getAllPatchPointData(patches);
        //  GmsRenderer::highlightSelectedPoint(12);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);

        glDrawArrays(GL_POINTS, 0, allPatchPointData.size() / 5);
    }
}

void PatchRenderer::updatePatchData(std::vector<Patch> &patches)
{
    if (GmsWindow::isClicked)
    {
        int selectedPatch = getSelectedPatch(patches, GmsWindow::mousePos);
        appState.selectedPatchId = selectedPatch;
        if (selectedPatch != -1)
            appState.currentPatchData = patches[selectedPatch].getControlMatrix();
        // GmsWindow::selectedPoint = getSelectedPointId(patches, GmsWindow::mousePos);
        //  gmsgui::setCurrentColor(getColorAtPoint(curves, GmsWindow::selectedPoint));
        GmsWindow::isClicked = false;
    }
    /*
    else if (GmsWindow::isDragging && GmsWindow::validSelectedPoint())
    {
        setPatchCoords(patches, GmsWindow::selectedPoint, GmsWindow::mousePos);
        //  size_t offset = GmsWindow::selectedPoint.primitiveId * 4 * sizeof(glm::vec2) + (GmsWindow::selectedPoint.pointId * sizeof(glm::vec2));
        //  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec2), &GmsWindow::mousePos);
    }
    */
}

void PatchRenderer::render(std::vector<Patch> &patches)
{
    GmsRenderer::render();
    updatePatchData(patches);
    renderPatches(patches);

    /*
        if (gui.patchColorChange)
        {
            patches[0].setControlMatrix(gui.currentPatchData);
            gui.patchColorChange = false;
        }
        else
        {
            gui.currentPatchData = patches[0].getControlMatrix();
        }
        **/
}