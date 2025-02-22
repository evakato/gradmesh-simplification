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
        glLineWidth(1.0f);
        drawPrimitive(params.glPatches, appState.patchRenderResources.patchShaderId, projectionMatrix, VERTS_PER_PATCH);
    }

    if (appState.componentSelectOptions.renderMaxProductRegion())
    {
        auto selectedRegion = appState.findSelectedRegion();
        if (selectedRegion)
        {
            auto &allRegions = (*selectedRegion).allRegionAttributes;
            if (appState.selectedTPRIdx < allRegions.size() && appState.selectedTPRIdx >= 0)
            {
                auto selectedTPR = allRegions[appState.selectedTPRIdx];
                appState.showProductRegion((*selectedRegion).gridPair, selectedTPR.maxRegion);
            }
        }
    }

    if (appState.renderCurves)
    {
        setVertexData(params.glCurves);
        glLineWidth(appState.curveLineWidth);
        // drawPrimitive(params.glCurves, curveShaderId, projectionMatrix, VERTS_PER_CURVE);
        glUseProgram(curveShaderId);
        setUniformProjectionMatrix(curveShaderId, projectionMatrix);

        glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);

        for (int i = 0; i < params.glCurves.size() / (5); i++) // Divide by total floats per curve
        {
            int r = ((i * VERTS_PER_CURVE) * 5) + 2;
            int g = ((i * VERTS_PER_CURVE) * 5) + 3;
            int b = ((i * VERTS_PER_CURVE) * 5) + 4;
            if (params.glCurves[r] >= 0.9f || params.glCurves[g] >= 0.9f || params.glCurves[b] >= 0.9f)
            {
                glLineWidth(appState.curveLineWidth * 2);
            }
            else
            {
                glLineWidth(appState.curveLineWidth);
            }

            glDrawArrays(GL_PATCHES, i * VERTS_PER_CURVE, VERTS_PER_CURVE);
        }
    }

    if (appState.componentSelectOptions.renderPatchAABB() && appState.userSelectedId.patchId != -1)
    {
        auto &selectedPatch = appState.patches[appState.userSelectedId.patchId];
        // auto aabbData = getAllCurveGLData(selectedPatch.getCurves(), &Curve::getGLAABBData);
        auto aabbData = selectedPatch.getGLAABBData();
        setVertexData(aabbData);
        glUseProgram(lineShaderId);
        setLineColor(lineShaderId, yellow);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        for (int i = 0; i < aabbData.size() / 5; i++)
            glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }

    if (appState.renderHandles)
    {
        const std::vector<GLfloat> pointHandleData = getAllHandleGLPoints(params.handles);
        setVertexData(pointHandleData);
        glUseProgram(lineShaderId);
        glLineWidth(appState.handleLineWidth);
        setUniformProjectionMatrix(lineShaderId, projectionMatrix);
        glDrawArrays(GL_LINES, 0, pointHandleData.size() / 2);

        const std::vector<GLfloat> allPatchHandleData = getAllHandleGLPoints(params.handles, 1, 2);
        setVertexData(allPatchHandleData);
        glUseProgram(pointShaderId);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);
        GLint pointSizeLocation = glGetUniformLocation(pointShaderId, "pointSize");
        glUniform1f(pointSizeLocation, 10.0f);
        // GmsRenderer::highlightSelectedPoint(12);
        glDrawArrays(GL_POINTS, 0, allPatchHandleData.size() / 5);
    }

    if (appState.renderControlPoints)
    {
        const std::vector<GLfloat> allPatchPointData = getAllPatchGLControlPointData(params.points, white);
        setVertexData(allPatchPointData);
        glUseProgram(pointShaderId);
        //  GmsRenderer::highlightSelectedPoint(12);
        setUniformProjectionMatrix(pointShaderId, projectionMatrix);

        GLint pointSizeLocation = glGetUniformLocation(pointShaderId, "pointSize");
        glUniform1f(pointSizeLocation, 20.0f);

        glDrawArrays(GL_POINTS, 0, allPatchPointData.size() / 5);
    }
}

void PatchRenderer::resetPreviousSelection()
{
    if (!appState.userSelectedId.isNull())
    {
        if (appState.manualEdgeSelect)
            appState.resetSelectedDhe();
        else if (appState.isCompSelect(ComponentSelectOptions::Type::Curve))
            appState.setUserCurveColor(black);
    }
}

void PatchRenderer::updatePatches(std::vector<Patch> &patches)
{
    if (!GmsWindow::isClicked)
        return;

    GmsWindow::isClicked = false;
    if (!appState.componentSelectOptions.on && !appState.manualEdgeSelect)
        return;

    resetPreviousSelection();

    glm::mat4 inverseProjectionMatrix = glm::inverse(projectionMatrix);
    glm::vec4 transformedPoint = inverseProjectionMatrix * glm::vec4{GmsWindow::mousePos, 0.0f, 1.0f};
    int prevPatchId = appState.userSelectedId.patchId;
    int newPatchId = getSelectedPatch(patches, transformedPoint);
    appState.setPatchId(newPatchId);
    if (appState.userSelectedId.patchId == -1 || (appState.componentSelectOptions.type == ComponentSelectOptions::Type::Patch && !appState.manualEdgeSelect))
        return; // only selected patch was modified (not curve)

    auto &selectedPatch = patches[newPatchId];
    int newCurveId = selectedPatch.getContainingCurve(transformedPoint);
    CurveId newUserCurve = CurveId{newPatchId, newCurveId};

    if (appState.manualEdgeSelect && newCurveId != -1)
    {
        appState.setSelectedDhe(newUserCurve);
        appState.updateCurves({});
    }
    else if (newCurveId != -1)
    {
        appState.setPatchCurveColor(newUserCurve, blue);
        appState.updateCurves({prevPatchId, newPatchId});
    }

    appState.userSelectedId = newUserCurve;
}

void PatchRenderer::render(PatchRenderParams &params)
{
    GmsRenderer::render();
    updatePatches(appState.patches);
    renderPatches(params);
}