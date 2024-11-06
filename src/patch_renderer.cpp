#include "patch_renderer.hpp"
#include "gms_app.hpp"
#include "stb_image_write.h"

PatchRenderer::PatchRenderer(GmsWindow &window, GmsAppState &appState) : GmsRenderer(window, appState)
{
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    appState.patchRenderResources.patchShaderId = linkShaders(patchShaders);
}

PatchRenderer::~PatchRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void PatchRenderer::renderPatches(PatchRenderParams &params)
{
    if (appState.renderPatches)
    {
        // same with this call
        GmsRenderer::setVertexData(params.glPatches);
        glUseProgram(appState.patchRenderResources.patchShaderId);
        setUniformProjectionMatrix(appState.patchRenderResources.patchShaderId, projectionMatrix);
        glLineWidth(1.0f);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
        glDrawArrays(GL_PATCHES, 0, params.glPatches.size() / 5);
    }

    if (appState.renderCurves)
    {
        GmsRenderer::setVertexData(params.glCurves);
        glUseProgram(curveShaderId);
        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
        glLineWidth(appState.curveLineWidth);
        setUniformProjectionMatrix(curveShaderId, projectionMatrix);
        glDrawArrays(GL_PATCHES, 0, params.glCurves.size() / 5);
    }

    if (appState.componentSelectOptions.renderPatchAABB() && appState.userSelectedId.patchId != -1)
    {
        auto &selectedPatch = appState.patches[appState.userSelectedId.patchId];
        // auto aabbData = getAllCurveGLData(selectedPatch.getCurves(), &Curve::getGLAABBData);
        auto aabbData = selectedPatch.getGLAABBData();
        GmsRenderer::setVertexData(aabbData);
        glUseProgram(lineShaderId);
        setLineColor(lineShaderId, blue);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        for (int i = 0; i < aabbData.size() / 5; i++)
            glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }

    if (appState.renderHandles)
    {
        const std::vector<GLfloat> pointHandleData = getAllHandleGLPoints(params.handles);
        GmsRenderer::setVertexData(pointHandleData);
        glUseProgram(lineShaderId);
        glLineWidth(appState.handleLineWidth);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        glDrawArrays(GL_LINES, 0, pointHandleData.size() / 2);

        const std::vector<GLfloat> allPatchHandleData = getAllHandleGLPoints(params.handles, 1, 2);
        GmsRenderer::setVertexData(allPatchHandleData);
        glUseProgram(pointShaderId);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);
        // GmsRenderer::highlightSelectedPoint(12);
        glDrawArrays(GL_POINTS, 0, allPatchHandleData.size() / 5);
    }

    if (appState.renderControlPoints)
    {
        const std::vector<GLfloat> allPatchPointData = getAllPatchGLControlPointData(params.points, white);
        GmsRenderer::setVertexData(allPatchPointData);
        glUseProgram(pointShaderId);
        //  GmsRenderer::highlightSelectedPoint(12);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);

        glDrawArrays(GL_POINTS, 0, allPatchPointData.size() / 5);
    }
}

void PatchRenderer::updatePatches(std::vector<Patch> &patches)
{
    if (!GmsWindow::isClicked)
        return;

    GmsWindow::isClicked = false;
    if (!appState.componentSelectOptions.on && !appState.manualEdgeSelect)
        return;

    if (!appState.userSelectedId.isNull())
    {
        appState.setPatchCurveColor(appState.userSelectedId, black);
        appState.resetSelectedDhe();
    }
    glm::mat4 inverseProjectionMatrix = glm::inverse(projectionMatrix);
    glm::vec4 transformedPoint = inverseProjectionMatrix * glm::vec4{GmsWindow::mousePos, 0.0f, 1.0f};

    appState.userSelectedId.patchId = getSelectedPatch(patches, transformedPoint);
    if (appState.userSelectedId.patchId == -1 || (appState.componentSelectOptions.type == ComponentSelectOptions::Type::Patch && !appState.manualEdgeSelect))
        return;

    auto &selectedPatch = patches[appState.userSelectedId.patchId];
    int curveIdx = selectedPatch.getContainingCurve(transformedPoint);
    CurveId newSelectedCurve = CurveId{appState.userSelectedId.patchId, curveIdx};

    appState.userSelectedId = newSelectedCurve;
    if (curveIdx != -1 && appState.manualEdgeSelect)
    {
        appState.setSelectedDhe();
    }
    else if (curveIdx != -1)
        appState.setPatchCurveColor(appState.userSelectedId, yellow);

    appState.patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
}

void PatchRenderer::render(PatchRenderParams &params)
{
    GmsRenderer::render();
    updatePatches(appState.patches);
    renderPatches(params);
}