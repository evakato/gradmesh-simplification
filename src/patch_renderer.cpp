#include "patch_renderer.hpp"

PatchRenderer::PatchRenderer(GmsWindow &window, GmsGui &gui, std::vector<Patch> &patchData) : GmsRenderer(window, gui), patches(patchData)
{
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    setupShaders();

    GmsRenderer::setVertexData(getAllPatchVertexData(patches));
}

PatchRenderer::~PatchRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

const void PatchRenderer::renderPatches()
{
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glPolygonMode(GL_FRONT_AND_BACK, gui.isWireframeMode ? GL_LINE : GL_FILL);

    if (gui.drawPatches)
    {
        // same with this call
        // GmsRenderer::setVertexData(patches[0].getGLControlMatrixData());
        GmsRenderer::setVertexData(getAllPatchVertexData(patches));
        // render patches
        glUseProgram(gradMeshShaderId);
        glLineWidth(1.0f);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
        for (int i = 0; i < patches.size(); i++)
            glDrawArrays(GL_PATCHES, i * VERTS_PER_PATCH, VERTS_PER_PATCH);
    }

    // curves, lines, and points
    GmsRenderer::setVertexData(getAllPatchCoordData(patches));

    if (gui.drawCurves)
    {
        std::vector<unsigned int> curveEBO = generateCurveEBO(patches.size());

        glUseProgram(curveShaderId);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, curveEBO.size() * sizeof(unsigned int), curveEBO.data(), GL_STATIC_DRAW);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
        glLineWidth(gui.curveLineWidth);
        glDrawElements(GL_PATCHES, 16 * patches.size(), GL_UNSIGNED_INT, 0);
    }

    if (gui.drawHandles)
    {
        std::vector<unsigned int> handleEBO = generateHandleEBO(patches.size());
        glUseProgram(lineShaderId);
        // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(handleIndicesForPatch), handleIndicesForPatch, GL_STATIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, handleEBO.size() * sizeof(unsigned int), handleEBO.data(), GL_STATIC_DRAW);
        glLineWidth(gui.handleLineWidth);
        glDrawElements(GL_LINES, 16 * patches.size(), GL_UNSIGNED_INT, 0);

        glUseProgram(pointShaderId);
        GmsRenderer::highlightSelectedPoint(12);
        for (int i = 0; i < patches.size(); i++)
            glDrawArrays(GL_POINTS, i * 12 + 4, 8);
    }

    if (gui.drawControlPoints)
    {
        glUseProgram(pointShaderId);
        GmsRenderer::highlightSelectedPoint(12);
        for (int i = 0; i < patches.size(); i++)
            glDrawArrays(GL_POINTS, i * 12, 4);
    }
}

const void PatchRenderer::updatePatchData()
{
    if (GmsWindow::isClicked)
    {
        GmsWindow::selectedPoint = getSelectedPointId(patches, GmsWindow::mousePos);
        // gmsgui::setCurrentColor(getColorAtPoint(curves, GmsWindow::selectedPoint));
        GmsWindow::isClicked = false;
    }
    else if (GmsWindow::isDragging && GmsWindow::validSelectedPoint())
    {
        setPatchCoords(patches, GmsWindow::selectedPoint, GmsWindow::mousePos);
        //  size_t offset = GmsWindow::selectedPoint.primitiveId * 4 * sizeof(glm::vec2) + (GmsWindow::selectedPoint.pointId * sizeof(glm::vec2));
        //  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(glm::vec2), &GmsWindow::mousePos);
    }
}

const void PatchRenderer::render()
{
    GmsRenderer::render();
    updatePatchData();
    renderPatches();

    if (gui.patchColorChange)
    {
        patches[0].setControlMatrix(gui.currentPatchData);
        gui.patchColorChange = false;
    }
    else
    {
        gui.currentPatchData = patches[0].getControlMatrix();
    }
}

const void PatchRenderer::setupShaders()
{
    std::vector gradMeshShaders{
        ShaderSource{"../shaders/patch.vs.glsl", GL_VERTEX_SHADER},
        ShaderSource{"../shaders/patch.tcs.glsl", GL_TESS_CONTROL_SHADER},
        ShaderSource{"../shaders/patch.tes.glsl", GL_TESS_EVALUATION_SHADER},
        ShaderSource{"../shaders/patch.fs.glsl", GL_FRAGMENT_SHADER},
    };

    gradMeshShaderId = linkShaders(gradMeshShaders);
}