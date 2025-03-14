#include "merging.hpp"
#include "gradmesh.hpp"
#include "preprocessing.hpp"
#include <omp.h>

GradMeshMerger::GradMeshMerger(GmsAppState &appState) : mesh(appState.mesh), appState(appState), metrics{MergeMetrics::Params{appState.mesh, appState.mergeSettings, appState.patchRenderResources}}, select{appState} {}

void GradMeshMerger::startupMesh()
{
    mesh.findULPoints();
    select.reset();
    select.findCandidateMerges();
    metrics.setAABB();
    metrics.captureGlobalImage(appState.patchRenderParams.glPatches, ORIG_IMG);
    metrics.setValenceVertices();
}

float GradMeshMerger::attemptMerge(int halfEdgeIdx, AABB &aabb)
{
    metrics.captureBeforeMerge(appState.originalGlPatches, aabb);
    mergePatches(halfEdgeIdx);
    auto patches = mesh.generatePatches();
    if (!patches)
        return 1.0f;

    auto glPatches = getAllPatchGLData(patches.value(), &Patch::getControlMatrix);
    return metrics.getMergeError(glPatches, CURR_IMG);
}

void GradMeshMerger::previewMerge()
{
    int edgeIdx = appState.candidateMerges[appState.selectedEdgeId].getHalfEdgeIdx();
    appState.mergePreviewError = attemptMerge(edgeIdx, appState.mergeSettings.aabb);
    appState.mesh = readHemeshFile("mesh_saves/save_" + std::to_string(appState.numOfMerges) + ".hemesh");
    appState.updateMeshRender();
    appState.mergeProcess = MergeProcess::Merging;
}

void GradMeshMerger::merge()
{
    if (appState.candidateMerges.size() <= 0 || appState.mergeMode == NONE)
    {
        if (appState.startTime.has_value())
        {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - appState.startTime.value();
            std::cout << "time: " << elapsed.count() << std::endl;
            appState.startTime.reset();
        }
        appState.mergeMode = NONE;
        return;
    }

    int selectedHalfEdgeIdx = select.selectEdge();

    if (selectedHalfEdgeIdx == -1)
    {
        appState.mergeStatus = NA;
        appState.mergeMode = NONE;
        metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
        appState.mergeError = metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
        return;
    }
    if (!mesh.validMergeEdge(selectedHalfEdgeIdx))
    {
        appState.mergeStatus = CYCLE;
        return;
    }
    auto &edge = mesh.edges[selectedHalfEdgeIdx];
    assert(edge.isValid());

    appState.mergeStatus = mergeAtSelectedEdge(selectedHalfEdgeIdx);

    if (appState.mergeMode == MANUAL)
    {
        appState.mergeMode = NONE;
        if (appState.candidateMerges.size() > 0)
            appState.selectedEdgeId = 0;

        if (appState.mergeStatus == SUCCESS)
            appState.userSelectedId = {-1, -1};
    }
}

MergeStatus GradMeshMerger::mergeAtSelectedEdge(int halfEdgeIdx)
{
    assert(!appState.candidateMerges.empty());

    writeLogFile(mesh, "debug1.txt");

    auto aabb = mesh.getAffectedMergeAABB(halfEdgeIdx);
    metrics.captureBeforeMerge(appState.originalGlPatches, aabb);
    GmsAppState::MergeStats stats = mergePatches(halfEdgeIdx);
    writeLogFile(mesh, "debug2.txt");

    auto mergedPatches = mesh.generatePatches();
    if (!mergedPatches)
    {
        appState.mesh = readHemeshFile("mesh_saves/save_" + std::to_string(appState.numOfMerges) + ".hemesh");
        appState.updateMeshRender();
        select.findCandidateMerges();
        return CYCLE;
    }

    auto glPatches = getAllPatchGLData(mergedPatches.value(), &Patch::getControlMatrix);
    if (appState.useError)
    {
        appState.mergeError = metrics.getMergeError(glPatches, MERGE_METRIC_IMG);
    }
    if (!appState.useError || appState.mergeError < appState.mergeSettings.errorThreshold)
    {
        appState.updateMeshRender(mergedPatches.value(), glPatches);
        appState.mergeStats = stats;
        appState.currentSave = ++appState.numOfMerges;
        writeHemeshFile("mesh_saves/save_" + std::to_string(appState.currentSave) + ".hemesh", mesh);
        metrics.captureGlobalImage(glPatches, CURR_IMG);
        select.findCandidateMerges();
        return SUCCESS;
    }
    appState.mesh = readHemeshFile("mesh_saves/save_" + std::to_string(appState.numOfMerges) + ".hemesh");
    appState.updateMeshRender();
    select.findCandidateMerges();
    return METRIC_ERROR;
}

GmsAppState::MergeStats GradMeshMerger::mergePatches(int mergeEdgeIdx)
{
    GmsAppState::MergeStats stats;
    auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = mesh.getFaceEdgeIdxs(mergeEdgeIdx);
    auto &face1R = mesh.edges[face1RIdx];
    auto &face1B = mesh.edges[face1BIdx];
    auto &face1L = mesh.edges[face1LIdx];
    auto &face1T = mesh.edges[face1TIdx];

    auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = mesh.getFaceEdgeIdxs(face1R.twinIdx);
    auto &face2L = mesh.edges[face2LIdx];
    auto &face2T = mesh.edges[face2TIdx];
    auto &face2R = mesh.edges[face2RIdx];
    auto &face2B = mesh.edges[face2BIdx];

    auto *topLeftEdge = &face1T;
    auto *topRightEdge = &face2T;
    auto *bottomLeftEdge = &face1B;
    auto *bottomRightEdge = &face2B;
    int newTopEdgeIdx = face1TIdx;
    int newBottomEdgeIdx = face1BIdx;

    bool scaleTopHandles = true;
    bool scaleBottomHandles = true;
    bool addTopT = true;
    bool addBottomT = true;

    bool topLeftL = face1L.isBar() && face1T.isStem();
    bool topLeftT = face1T.isBar() && (mesh.twinIsStem(face1L) || mesh.twinParentIsStem(face1L));
    bool topRightT = face2T.isBar() && (face2R.isStem() || mesh.parentIsStem(face2R));
    bool topRightL = face2R.isBar() && (mesh.twinIsStem(face2T) || mesh.twinParentIsStem(face2T)) && !topRightT;
    bool topIsStem = face1R.isStem();
    stats.topEdgeCase = getCornerTJunctions(topLeftL, topLeftT, topRightL, topRightT, topIsStem);

    bool bottomLeftT = (face1L.isStem() || mesh.parentIsStem(face1L)) && face1B.isBar();
    bool bottomRightL = face2R.isBar() && face2B.isStem();
    bool bottomRightT = face2B.isBar() && (mesh.twinIsStem(face2R) || mesh.twinParentIsStem(face2R));
    bool bottomIsStem = face2L.isStem();
    stats.bottomEdgeCase = getCornerTJunctions(0, bottomLeftT, bottomRightL, bottomRightT, bottomIsStem);

    float topEdgeT = topIsStem ? 0 : splittingFactor(face1R, face1T, face2T, 1);
    float bottomEdgeT = bottomIsStem ? 0 : splittingFactor(face2L, face1B, face2B, 1);
    float t = (topIsStem && bottomIsStem) ? 0 : (topEdgeT + bottomEdgeT) / (!topIsStem + !bottomIsStem);

    mesh.disablePoint(face1R);
    mesh.disablePoint(face1B);
    mesh.disablePoint(face2T);
    mesh.disablePoint(face2L);

    switch (stats.topEdgeCase)
    {
    case IsStem:
    {
        if (face1T.isLeftMostChild() && face2T.isRightMostChild())
        {
            childBecomesItsParent(face1TIdx);
        }
        else
        {
            face1T.interval.y = face2T.interval.y;
            setNextRightL(face2T, face1RIdx);
            auto &parent = mesh.edges[face1T.parentIdx];
            parent.removeChildIdx(face1RIdx);
            parent.removeChildIdx(face2TIdx);
        }
        transferChildTo(face2RIdx, face1RIdx);
        scaleTopHandles = addTopT = 0;
        break;
    }
    case LeftT | RightT:
    {
        // 9 --> 18 --> 8
        newTopEdgeIdx = face1T.parentIdx;
        topLeftEdge = &mesh.edges[newTopEdgeIdx];
        topRightEdge = &mesh.edges[face2T.parentIdx];

        float totalRelativeLeft = totalCurveRelativeLeft((1.0f - t) / t, face1T, face2T);
        float totalRelativeRight = totalCurveRelativeRight(t / (1.0f - t), face2T, face1T);
        topEdgeT = 1.0f / totalRelativeLeft;

        rightTUpdateInterval(face2T.parentIdx, totalRelativeRight - 1.0f, totalRelativeRight);
        leftTUpdateInterval(newTopEdgeIdx, totalRelativeLeft);
        setChildrenNewParent(*topRightEdge, newTopEdgeIdx);

        // left T inherits the children from right T
        topRightEdge->replaceChild(face2TIdx, face1TIdx);
        topRightEdge->replaceChild(face2RIdx, face1RIdx);
        topLeftEdge->addChildrenIdxs(topRightEdge->childrenIdxs);
        topLeftEdge->nextIdx = topRightEdge->nextIdx;

        face1R.createStem(newTopEdgeIdx, face2R.interval);
        face1T.interval.y = face2T.interval.y;
        topRightEdge->disable();
        break;
    }
    case LeftL | RightT:
        // if top left edge is a stem, turn the parent into that stem
        // 2 --> 20 --> 7 (no twisting head) or 3 --> 24 --> 36
        transferChildTo(face1TIdx, face2T.parentIdx);
        [[fallthrough]];
    case RightT:
    {
        // 41 --> 23
        // keep the parent, make the top left edge the bar1
        newTopEdgeIdx = face2T.parentIdx;
        topRightEdge = &mesh.edges[newTopEdgeIdx];

        auto [newCurvePart, totalCurve] = parameterizeTBar2(t / (1.0f - t), face2T);
        topEdgeT = newCurvePart / totalCurve;

        mesh.handles[topRightEdge->handleIdxs.first] = mesh.handles[face1T.handleIdxs.first] * (1.0f / topEdgeT);
        mesh.handles[topRightEdge->handleIdxs.second] *= (1.0f / (1.0f - topEdgeT));

        topRightEdge->copyGeometricData(face1T);
        face1T.createBar(-1, {0, 1});
        transferChildToWithoutGeometry(face2TIdx, face1TIdx);
        rightTUpdateInterval(newTopEdgeIdx, newCurvePart, totalCurve);
        transferChildTo(face2RIdx, face1RIdx);

        scaleTopHandles = false;
        break;
    }
    case LeftT | RightL:
        // 9 --> 47 --> 8 or 16 -> 38 --> 26
        transferChildTo(face2RIdx, face1RIdx);
        [[fallthrough]];
    case LeftT:
    {
        newTopEdgeIdx = face1T.parentIdx;
        topLeftEdge = &mesh.edges[newTopEdgeIdx];

        auto [_, totalCurve] = parameterizeTBar1((1.0f - t) / t, face1T);
        topEdgeT = 1.0f / totalCurve;

        leftTUpdateInterval(newTopEdgeIdx, totalCurve);
        break;
    }
    case LeftL | RightL:
    case RightL:
    {
        // for leftL, weird case: basically just delete the stem from the RightL
        // 2 --> 49 --> 7 or 18 --> 35 --> 16
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        break;
    }
    default:
    {
        break;
    }
    }

    switch (stats.bottomEdgeCase)
    {
    case IsStem:
    {
        if (face2B.isLeftMostChild() && face1B.isRightMostChild())
        {
            childBecomesItsParent(face1BIdx);
        }
        else
        {
            face1B.interval.x = face2B.interval.x;
            face1B.color = face2B.color;
            auto &parent = mesh.edges[face1B.parentIdx];
            parent.removeChildIdx(face2BIdx);
            parent.removeChildIdx(face2LIdx);
        }
        if (!topIsStem)
            transferChildTo(face2RIdx, face1RIdx);

        scaleBottomHandles = addBottomT = 0;
        break;
    }
    case LeftT | RightT:
    {
        // 0 -> 20 -> 14
        newBottomEdgeIdx = face1B.parentIdx;
        bottomLeftEdge = &mesh.edges[newBottomEdgeIdx];
        int rightTParentIdx = face2B.parentIdx;
        bottomRightEdge = &mesh.edges[rightTParentIdx];

        float totalRelativeRight = totalCurveRelativeRight((1.0f - t) / t, face1B, face2B);
        float totalRelativeLeft = totalCurveRelativeLeft(t / (1.0f - t), face2B, face1B);
        bottomEdgeT = 1.0f / totalRelativeRight;

        leftTUpdateInterval(rightTParentIdx, totalRelativeLeft);
        rightTUpdateInterval(newBottomEdgeIdx, totalRelativeRight - 1.0f, totalRelativeRight);
        setChildrenNewParent(*bottomRightEdge, newBottomEdgeIdx);

        bottomRightEdge->replaceChild(face2BIdx, face1BIdx);
        bottomLeftEdge->addChildrenIdxs(bottomRightEdge->childrenIdxs);

        face1B.interval.x = face2B.interval.x;
        face1B.copyGeometricData(face2B);
        transferChildTo(rightTParentIdx, newBottomEdgeIdx);
        bottomRightEdge->disable();
        break;
    }
    case RightT:
    {
        // 1 -> 31 -> 9 the bottom left edge is not a stem.. its twin is a stem
        // the bottom left edge inherits the bar data from the bottom right edge
        newBottomEdgeIdx = face2B.parentIdx;
        bottomRightEdge = &mesh.edges[newBottomEdgeIdx];

        auto [newCurvePart, totalCurve] = parameterizeTBar1(t / (1.0f - t), face2B);
        bottomEdgeT = newCurvePart / totalCurve;

        mesh.handles[bottomRightEdge->handleIdxs.first] *= (1.0f / (1.0f - bottomEdgeT));
        mesh.handles[bottomRightEdge->handleIdxs.second] = mesh.handles[face1B.handleIdxs.second] * (1.0f / bottomEdgeT);
        bottomRightEdge->nextIdx = face1LIdx; // important !!!

        face1B.createBar(-1, {0, 1});
        transferChildTo(face2BIdx, face1BIdx);

        leftTUpdateInterval(newBottomEdgeIdx, totalCurve);
        scaleBottomHandles = false;
        break;
    }
    case LeftT | RightL:
    {
        // 1 -> 24 -> 31 or 0 --> 47 --> 14 or 17 --> 37 --> 23
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        transferChildToWithoutGeometry(face2BIdx, face1B.parentIdx);
        [[fallthrough]];
    }
    case LeftT:
    {
        newBottomEdgeIdx = face1B.parentIdx;
        bottomLeftEdge = &mesh.edges[newBottomEdgeIdx];

        float newCurvePart = (1.0f - t) / t * face1B.interval.y;
        float totalCurve = 1.0f + newCurvePart;
        bottomEdgeT = 1.0f / totalCurve;

        rightTUpdateInterval(newBottomEdgeIdx, newCurvePart, totalCurve);
        face1B.interval.x = 0;
        face1B.color = face2B.color;
        break;
    }
    case RightL:
    {
        // with LeftL: 2 --> 49 --> 16
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        transferChildTo(face2BIdx, face1BIdx);
        break;
    }
    default:
    {
        break;
    }
    }

    if (scaleTopHandles)
    {
        // assert(topLeftEdge->handleIdxs.first != -1 && topLeftEdge->handleIdxs.second != -1);
        if (topLeftEdge->handleIdxs.first == -1 || topLeftEdge->handleIdxs.second == -1 || topRightEdge->handleIdxs.second == -1)
        {
            std::cout << "fail" << stats.topEdgeCase << std::endl;
            // assert(topRightEdge->handleIdxs.second != -1);
        }
        else
        {

            float topLeftScale = 1.0f / topEdgeT;
            float topRightScale = 1.0f / (1.0f - topEdgeT);
            auto &topLeftHandle = mesh.handles[topLeftEdge->handleIdxs.first];
            auto &topRightHandle = mesh.handles[topLeftEdge->handleIdxs.second];
            topLeftHandle *= topLeftScale;
            topRightHandle = mesh.handles[topRightEdge->handleIdxs.second] * topRightScale;
            face1R.copyGeometricData(face2R);
        }
    }

    if (scaleBottomHandles)
    {
        // assert(bottomLeftEdge->handleIdxs.first != -1 && bottomLeftEdge->handleIdxs.second != -1);
        if (bottomLeftEdge->handleIdxs.first == -1 || bottomLeftEdge->handleIdxs.second == -1 || bottomRightEdge->handleIdxs.second == -1)
        {
            std::cout << "fail2" << std::endl;
        }
        else
        {

            float bottomLeftScale = 1.0f / bottomEdgeT;
            float bottomRightScale = 1.0f / (1.0f - bottomEdgeT);
            auto &bottomLeftHandle = mesh.handles[bottomLeftEdge->handleIdxs.second];
            auto &bottomRightHandle = mesh.handles[bottomLeftEdge->handleIdxs.first];
            bottomLeftHandle *= bottomLeftScale;
            bottomRightHandle = mesh.handles[bottomRightEdge->handleIdxs.first] * bottomRightScale;
            bottomLeftEdge->copyGeometricData(*bottomRightEdge);
        }
    }

    face1R.handleIdxs = face2R.handleIdxs;
    setNextRightL(face2R, face1BIdx);

    if (addTopT)
        addTJunction(*topRightEdge, *topLeftEdge, newTopEdgeIdx, 1.0f - topEdgeT);
    if (addBottomT)
        addTJunction(*bottomLeftEdge, *bottomRightEdge, newBottomEdgeIdx, bottomEdgeT);

    copyEdgeTwin(face1RIdx, face2RIdx);
    removeFace(face2L.faceIdx);

    stats.mergedHalfEdgeIdx = mergeEdgeIdx;
    stats.t = t;
    stats.removedFaceId = face2L.faceIdx;
    stats.topEdgeT = 1.0f - topEdgeT;
    stats.bottomEdgeT = bottomEdgeT;
    return stats;
}

float GradMeshMerger::splittingFactor(HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign) const
{
    auto left = mesh.computeEdgeDerivatives(bar1);
    auto right = mesh.computeEdgeDerivatives(bar2);
    assert(left && right);
    const auto &leftPv01 = left.value()[2].coords;   // left patch: P_v(0,1)
    const auto &rightPv00 = right.value()[1].coords; // right patch: P_v(0,0)
    const auto &leftPuv01 = stem.twist.coords;       // left patch: P_uv(0,1)
    const auto &rightPuv00 = bar2.twist.coords;      // right patch: P_uv(0,0)

    auto sumPv0 = absSum(leftPv01, rightPv00); // P_v(0,1) + P_v(0,0)

    float r1 = glm::length(leftPv01) / glm::length(sumPv0);
    float r2 = glm::length(absSum(leftPv01, sign * BCM * leftPuv01)) / glm::length((sumPv0 + sign * BCM * absSum(leftPuv01, rightPuv00)));

    float t = 0.5 * (r1 + r2);
    // std::cout << "r1: " << r1 << " r2: " << r2 << " t: " << t << "\n";
    return t;
}

void GradMeshMerger::leftTUpdateInterval(int parentIdx, float totalCurve)
{
    auto &parentEdge = mesh.edges[parentIdx];
    for (int childIdx : parentEdge.childrenIdxs)
    {
        auto &child = mesh.edges[childIdx];
        if (child.parentIdx != parentIdx)
            continue; // strong check b/c i wrote some bad code

        if (child.isRightMostChild())
            child.interval.x /= totalCurve;
        else
            child.interval /= totalCurve;
    }
}

void GradMeshMerger::rightTUpdateInterval(int parentIdx, float reparam1, float reparam2)
{
    for (int childIdx : mesh.edges[parentIdx].childrenIdxs)
    {
        auto &child = mesh.edges[childIdx];
        if (child.parentIdx != parentIdx)
            continue; // strong check b/c i wrote some bad code

        if (child.isLeftMostChild())
        {
            child.interval.y += reparam1;
            child.interval.y /= reparam2;
        }
        else
        {
            child.interval += reparam1;
            child.interval /= reparam2;
        }
    }
}

void GradMeshMerger::scaleDownChildrenByT(HalfEdge &parentEdge, float t)
{
    for (int childIdx : parentEdge.childrenIdxs)
        mesh.edges[childIdx].interval *= t;
}

void GradMeshMerger::scaleUpChildrenByT(HalfEdge &parentEdge, float t)
{
    for (int childIdx : parentEdge.childrenIdxs)
    {
        mesh.edges[childIdx].interval *= (1 - t);
        mesh.edges[childIdx].interval += t;
    }
}

// I always forget how I wrote this function, bar1 and bar2 are the literal bar1 and bar2 of the new T-junction. That is the order.
bool GradMeshMerger::addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t)
{
    if (!edge1.hasTwin() || !edge2.hasTwin())
        return 0;

    auto twinHandles = mesh.edges[twinOfParentIdx].handleIdxs;
    int parentIdx;

    int bar1Idx = edge1.twinIdx;
    int bar2Idx = edge2.twinIdx;

    if (mesh.edges[bar1Idx].isBar())
        bar1Idx = mesh.edges[bar1Idx].parentIdx; // this is me being bad, the twin isn't updated to the parentIdx like it should be so I have to do a manual check

    if (mesh.edges[bar2Idx].isBar())
        bar2Idx = mesh.edges[bar2Idx].parentIdx; // same here

    int stemIdx = mesh.edges[bar1Idx].nextIdx;
    if (mesh.edges[mesh.edges[bar1Idx].nextIdx].isBar())
    {
        stemIdx = mesh.edges[mesh.edges[bar1Idx].nextIdx].parentIdx;
    }

    if (mesh.edges[bar2Idx].isParent() && mesh.edges[bar1Idx].isParent())
    {
        // strategy: remove the parent of bar2 and update bar1
        parentIdx = bar1Idx;
        scaleDownChildrenByT(mesh.edges[bar1Idx], t);
        scaleUpChildrenByT(mesh.edges[bar2Idx], t);
        mesh.edges[bar1Idx].addChildrenIdxs(mesh.edges[bar2Idx].childrenIdxs);
        setChildrenNewParent(mesh.edges[bar2Idx], parentIdx);
        mesh.edges[bar2Idx].disable();
    }
    else if (mesh.edges[bar1Idx].isParent())
    {
        parentIdx = bar1Idx;
        scaleDownChildrenByT(mesh.edges[bar1Idx], t);
        mesh.edges[bar1Idx].addChildrenIdxs({bar2Idx});
        mesh.edges[bar2Idx].createBar(parentIdx, {t, 1});
    }
    else if (mesh.edges[bar2Idx].isParent())
    {
        parentIdx = bar2Idx;
        scaleUpChildrenByT(mesh.edges[bar2Idx], t);
        mesh.edges[bar2Idx].addChildrenIdxs({bar1Idx});
    }
    else
    {
        parentIdx = mesh.addEdge(HalfEdge{});
        mesh.edges[parentIdx].childrenIdxs = {bar1Idx, bar2Idx};
        mesh.edges[bar2Idx].createBar(parentIdx, {t, 1});
    }

    if (!mesh.edges[bar1Idx].isParent())
    {
        if (mesh.edges[bar1Idx].isStem())
        {
            // 11 --> 40 -- > 23
            transferChildTo(bar1Idx, parentIdx);
            mesh.edges[parentIdx].handleIdxs = mesh.edges[bar1Idx].handleIdxs;
        }
        mesh.edges[parentIdx].copyGeometricData(mesh.edges[bar1Idx]);
        mesh.edges[bar1Idx].createBar(parentIdx, {0, t});
    }

    mesh.edges[stemIdx].createStem(parentIdx, {t, t});

    mesh.edges[parentIdx].handleIdxs = {twinHandles.second, twinHandles.first};
    mesh.edges[parentIdx].twinIdx = twinOfParentIdx;
    mesh.edges[parentIdx].nextIdx = mesh.edges[bar2Idx].nextIdx;
    mesh.edges[parentIdx].addChildrenIdxs({stemIdx});
    setBarChildrensTwin(mesh.edges[parentIdx], twinOfParentIdx);

    // mesh.edges[twinOfParentIdx].twinIdx = parentIdx;
    setParentChildrenTwin(mesh.edges[twinOfParentIdx], parentIdx);

    return 1;
}

void GradMeshMerger::setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx)
{
    for (int childIdx : parentEdge.childrenIdxs)
        mesh.edges[childIdx].parentIdx = newParentIdx;
}

void GradMeshMerger::setParentChildrenTwin(HalfEdge &parentEdge, int newTwinIdx)
{
    if (parentEdge.twinIdx == -1 || newTwinIdx == -1)
        return;
    parentEdge.twinIdx = newTwinIdx;

    for (int childIdx : parentEdge.childrenIdxs)
        if (mesh.edges[childIdx].isBar())
            mesh.edges[childIdx].twinIdx = newTwinIdx;
}

void GradMeshMerger::setBarChildrensTwin(HalfEdge &parentEdge, int twinIdx)
{
    for (int childIdx : parentEdge.childrenIdxs)
        if (mesh.edges[childIdx].isBar())
            mesh.edges[childIdx].twinIdx = twinIdx;
}

void GradMeshMerger::childBecomesItsParent(int childIdx)
{
    auto &child = mesh.edges[childIdx];
    int parentIdx = child.parentIdx;
    fixAndSetTwin(childIdx);
    transferChildTo(parentIdx, childIdx);
    child.handleIdxs = mesh.edges[parentIdx].handleIdxs;
    mesh.edges[parentIdx].disable();
}

void GradMeshMerger::setNextRightL(const HalfEdge &bar, int nextIdx)
{
    if (bar.isRightMostChild())
        mesh.edges[bar.parentIdx].nextIdx = nextIdx;
}

void GradMeshMerger::transferChildTo(int oldChildIdx, int newChildIdx)
{
    mesh.edges[newChildIdx].copyGeometricData(mesh.edges[oldChildIdx]);
    transferChildToWithoutGeometry(oldChildIdx, newChildIdx);
}

void GradMeshMerger::transferChildToWithoutGeometry(int oldChildIdx, int newChildIdx)
{
    auto &oldChild = mesh.edges[oldChildIdx];
    auto &newChild = mesh.edges[newChildIdx];
    newChild.copyChildData(oldChild);
    if (oldChild.parentIdx != -1)
        mesh.edges[oldChild.parentIdx].replaceChild(oldChildIdx, newChildIdx);
}

void GradMeshMerger::fixAndSetTwin(int barIdx)
{
    auto &bar = mesh.edges[barIdx];
    // std::cout << "parent is: " << bar.parentIdx;
    auto &parent = mesh.edges[bar.parentIdx];
    auto &twin = mesh.edges[parent.twinIdx];
    if (bar.twinIdx != parent.twinIdx)
    {
        // std::cout << "fixing the twin: prev: " << bar.twinIdx << " new: " << parent.twinIdx << "\n";
        bar.twinIdx = parent.twinIdx;
    }

    twin.twinIdx = barIdx;
    // std::cout << "setting " << bar.twinIdx << " to " << mesh.edges[bar.twinIdx].twinIdx << ".\n";
}

void GradMeshMerger::copyEdgeTwin(int e1Idx, int e2Idx)
{
    auto &e1 = mesh.edges[e1Idx];
    auto &e2 = mesh.edges[e2Idx];
    e1.twinIdx = e2.twinIdx;
    // don't set the twin if edge2 is a bar b/c it should point to the parent
    if (e2.hasTwin() && !e2.isBar())
    {
        auto &twin = mesh.edges[e2.twinIdx];
        if (twin.isParent())
        {
            for (int childIdx : twin.childrenIdxs)
            {
                if (mesh.edges[childIdx].isBar())
                    mesh.edges[childIdx].twinIdx = e1Idx;
            }
        }
        twin.twinIdx = e1Idx;
    }
}

void GradMeshMerger::removeFace(int faceIdx)
{
    auto &face = mesh.faces[faceIdx];
    auto &e1 = mesh.edges[face.halfEdgeIdx];
    auto &e2 = mesh.edges[e1.nextIdx];
    auto &e3 = mesh.edges[e2.nextIdx];
    auto &e4 = mesh.edges[e3.nextIdx];
    if (e1.handleIdxs.first != -1)
        mesh.handles[e1.handleIdxs.first].halfEdgeIdx = mesh.handles[e1.handleIdxs.second].halfEdgeIdx = -1;
    if (e3.handleIdxs.first != -1)
        mesh.handles[e3.handleIdxs.first].halfEdgeIdx = mesh.handles[e3.handleIdxs.second].halfEdgeIdx = -1;
    if (e4.handleIdxs.first != -1)
        mesh.handles[e4.handleIdxs.first].halfEdgeIdx = mesh.handles[e4.handleIdxs.second].halfEdgeIdx = -1;
    face.halfEdgeIdx = e1.faceIdx = e2.faceIdx = e3.faceIdx = e4.faceIdx = -1;
}
