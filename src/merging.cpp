#include "merging.hpp"
#include "gradmesh.hpp"

GradMeshMerger::GradMeshMerger(GmsAppState &appState, int patchShaderId) : mesh(appState.mesh), appState(appState), metrics{patchShaderId} {}

MergeState GradMeshMerger::mergeAtSelectedEdge()
{
    if (candidateMerges.empty() || appState.selectedEdgeId < 0 || appState.selectedEdgeId >= candidateMerges.size())
        return FAILURE;

    appState.merges.push_back(appState.selectedEdgeId);
    writeLogFile(mesh, appState, "debug1.txt");

    if (appState.saveMerges)
        writeMergeList(appState, "mergelist.txt");

    int halfEdgeIdx = candidateMerges[appState.selectedEdgeId].getHalfEdgeIdx();
    GmsAppState::MergeStats stats = mergePatches(halfEdgeIdx);

    saveImage((std::string{LOGS_DIR} + "/" + extractFileName(appState.filename) + ".png").c_str(), GL_LENGTH, GL_LENGTH);
    writeLogFile(mesh, appState, "debug2.txt");

    std::vector<Patch> mergedPatches = mesh.generatePatchData();
    auto glPatches = getAllPatchGLData(mergedPatches, &Patch::getControlMatrix);
    if (!appState.useError || metrics.doMerge(glPatches, appState.meshAABB, appState.errorThreshold))
    {
        appState.updateMeshRender(mergedPatches, glPatches);
        appState.mergeStats = stats;
        writeHemeshFile("mesh_saves/save_" + std::to_string(appState.merges.size()) + ".hemesh", mesh);
        std::rename(MERGE_METRIC_IMG, ORIG_METRIC_IMG);
        findCandidateMerges();
        return SUCCESS;
    }
    appState.merges.pop_back();
    std::cout << "Not gonna do merge" << std::endl;
    appState.mesh = readHemeshFile("mesh_saves/save_" + std::to_string(appState.merges.size()) + ".hemesh");
    appState.updateMeshRender();
    findCandidateMerges();
    return METRIC_ERROR;
}

void GradMeshMerger::findCandidateMerges()
{
    candidateMerges.clear();
    int faceIdx = 0;
    for (auto &face : mesh.getFaces())
    {
        if (face.isValid())
        {
            int currIdx = face.halfEdgeIdx;
            for (int i = 0; i < 4; i++)
            {
                const auto &currEdge = mesh.edges[currIdx];
                if (currEdge.hasTwin())
                {
                    auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = mesh.getFaceEdgeIdxs(currIdx);
                    auto &face1R = mesh.edges[face1RIdx];
                    auto &face1B = mesh.edges[face1BIdx];
                    auto &face1L = mesh.edges[face1LIdx];
                    auto &face1T = mesh.edges[face1TIdx];
                    auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = mesh.getFaceEdgeIdxs(face1R.twinIdx);
                    auto &face2L = mesh.edges[face2LIdx];
                    auto &face2T = mesh.edges[face2TIdx];
                    auto &face2R = mesh.edges[face2RIdx];
                    auto &face2B = mesh.edges[face2BIdx];

                    bool topLeftL = face1L.isBar() && face1T.isStem();
                    bool topLeftT = face1T.isBar() && (mesh.twinIsStem(face1L) || mesh.twinParentIsStem(face1L));
                    bool topRightL = face2R.isBar() && mesh.twinIsStem(face2T);
                    bool topRightT = face2T.isBar() && (face2R.isStem() || mesh.parentIsStem(face2R));
                    bool topIsStem = face1R.isStem();
                    int topEdge = getCornerTJunctions(topLeftL, topLeftT, topRightL, topRightT);

                    bool bottomLeftL = face1L.isBar() && mesh.twinIsStem(face1B);
                    bool bottomLeftT = (face1L.isStem() || mesh.parentIsStem(face1L)) && face1B.isBar();
                    bool bottomRightL = face2R.isBar() && face2B.isStem();
                    bool bottomRightT = face2B.isBar() && (mesh.twinIsStem(face2R) || mesh.twinParentIsStem(face2R));
                    bool bottomIsStem = face2L.isStem();
                    int bottomEdge = getCornerTJunctions(bottomLeftL, bottomLeftT, bottomRightL, bottomRightT);

                    bool currCycle1 = (topEdge & (LeftT | RightL)) && (bottomEdge & (RightT | LeftL));
                    bool currCycle2 = (topEdge & (LeftL | RightT)) && (bottomEdge & (LeftT | RightL));

                    bool adjFaceCycles = mesh.incidentFaceCycle(face1T.twinIdx) || mesh.incidentFaceCycle(face2T.twinIdx) || mesh.incidentFaceCycle(face1B.twinIdx) || mesh.incidentFaceCycle(face2B.twinIdx);

                    const auto &currFaceB = mesh.edges[currEdge.nextIdx];
                    const auto &currFaceT = mesh.edges[currEdge.prevIdx];
                    const auto &twinEdge = mesh.edges[currEdge.twinIdx];
                    const auto &otherFaceT = mesh.edges[twinEdge.nextIdx];
                    const auto &otherFaceB = mesh.edges[twinEdge.prevIdx];

                    bool hasRightCycles = twinFaceIsRightCycle(face1T) || twinFaceIsRightCycle(face2B);

                    bool bothStems = currEdge.isStem() && twinEdge.isStem();

                    bool hasAdjCyclesT = (twinFaceIsCycle(face2T, face1T) || twinFaceIsCycle(face1T, face2T)) && !topIsStem;
                    bool hasAdjCyclesB = (twinFaceIsCycle(face1B, face2B) || twinFaceIsCycle(face2B, face1B)) && !bottomIsStem;

                    bool hasAdjCycles2T = (twinFaceIsCCWCycle(face2T) || twinFaceIsCCWCycle(face1T)) && !topIsStem;
                    bool hasAdjCycles2B = (twinFaceIsCCWCycle(face1B) || twinFaceIsCCWCycle(face2B)) && !bottomIsStem;

                    // bool validStemT = currEdge.isStem() && !twinFaceIsCycle(currFaceB) && !twinFaceIsCycle(otherFaceB);
                    // bool validStemB = twinEdge.isStem() && !twinFaceIsCycle(currFaceT) && !twinFaceIsCycle(otherFaceT);

                    if (!currEdge.isBar() && !mesh.twinIsParent(currEdge) && !currEdge.isParent() && (!currCycle1 && !currCycle2 && !hasAdjCyclesT && !hasAdjCyclesB && !hasAdjCycles2T && !hasAdjCycles2B && !adjFaceCycles || bothStems))
                    {
                        auto findEdgeIdx = std::find_if(candidateMerges.begin(), candidateMerges.end(), [currIdx](const DoubleHalfEdge &dhe)
                                                        { return dhe.halfEdgeIdx2 == currIdx; });

                        if (findEdgeIdx == candidateMerges.end())
                        {
                            candidateMerges.push_back(DoubleHalfEdge{currIdx, currEdge.twinIdx, CurveId{faceIdx, i}});
                        }
                        else
                        {
                            size_t foundIndex = std::distance(candidateMerges.begin(), findEdgeIdx);
                            // assert(currIdx != candidateMerges[foundIndex].halfEdgeIdx2)
                            candidateMerges[foundIndex]
                                .curveId2 = CurveId{faceIdx, i};
                        }
                    }
                }
                currIdx = currEdge.nextIdx;
            }
            faceIdx++;
        }
    }

    for (auto &doublehe : candidateMerges)
    {
        // std::cout << doublehe.halfEdgeIdx1 << ", " << doublehe.halfEdgeIdx2 << "\n";
    }
    appState.numOfCandidateMerges = candidateMerges.size();
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

    // ya bitch can rly name shit well
    bool test = true;
    bool test2 = true;
    bool addTopT = true;
    bool addBottomT = true;

    bool topLeftL = face1L.isBar() && face1T.isStem();
    bool topLeftT = face1T.isBar() && (mesh.twinIsStem(face1L) || mesh.twinParentIsStem(face1L));
    bool topRightL = face2R.isBar() && mesh.twinIsStem(face2T);
    bool topRightT = face2T.isBar() && (face2R.isStem() || mesh.parentIsStem(face2R));
    bool topIsStem = face1R.isStem();
    int topEdge = getCornerTJunctions(topLeftL, topLeftT, topRightL, topRightT);

    bool bottomLeftT = (face1L.isStem() || mesh.parentIsStem(face1L)) && face1B.isBar();
    bool bottomRightL = face2R.isBar() && face2B.isStem();
    bool bottomRightT = face2B.isBar() && (mesh.twinIsStem(face2R) || mesh.twinParentIsStem(face2R));
    bool bottomIsStem = face2L.isStem();
    int bottomEdge = getCornerTJunctions(0, bottomLeftT, bottomRightL, bottomRightT);

    float topEdgeT = topIsStem ? 0 : splittingFactor(face1R, face1T, face2T, 1);
    float bottomEdgeT = bottomIsStem ? 0 : splittingFactor(face2L, face1B, face2B, 1);
    float t = (topIsStem && bottomIsStem) ? 0 : (topEdgeT + bottomEdgeT) / (!topIsStem + !bottomIsStem);

    mesh.disablePoint(face1R);
    mesh.disablePoint(face1B);

    stats.bottomEdgeCase = "None";
    stats.topEdgeCase = "None";

    bool alreadyCopied = false;

    if (topIsStem)
    {
        if (face1T.isLeftMostChild() && face2T.isRightMostChild())
        {
            stats.topEdgeCase = "Stem becomes parent";
            childBecomesItsParent(face1TIdx);
        }
        else
        {
            stats.topEdgeCase = "Stem becomes bar";
            face1T.interval.y = face2T.interval.y;
            setNextRightL(face2T, face1RIdx);
            auto &parent = mesh.edges[face1T.parentIdx];
            parent.removeChildIdx(face1RIdx);
            parent.removeChildIdx(face2TIdx);
        }
        transferChildTo(face2RIdx, face1RIdx);
        topEdge = addTopT = 0;
        test = false;
    }
    switch (topEdge)
    {
    case LeftT | RightT:
    {
        // 9 --> 18 --> 8
        stats.topEdgeCase = "LeftT and RightT";
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
        stats.topEdgeCase = "RightT (w/ or w/o LeftL)";
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

        alreadyCopied = true;
        test = false;
        break;
    }
    case LeftT | RightL:
        // 9 --> 47 --> 8 or 16 -> 38 --> 26
        // transferChildToWithoutGeometry(face2TIdx, face1T.parentIdx);
        transferChildTo(face2RIdx, face1RIdx);
        alreadyCopied = true;
        // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
        [[fallthrough]];
    case LeftT:
    {
        stats.topEdgeCase = "LeftT (w/ or w/o RightL)";
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
        stats.topEdgeCase = "RightL (w/ or w/o LeftL)";
        // for leftL, weird case: basically just delete the stem from the RightL
        // 2 --> 49 --> 7 or 18 --> 35 --> 16
        // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        break;
    }
    case LeftL:
    {
        stats.topEdgeCase = "LeftL only";
    }
    default:
    {
        break;
    }
    }

    if (bottomIsStem)
    {
        if (face2B.isLeftMostChild() && face1B.isRightMostChild())
        {
            stats.bottomEdgeCase = "Stem becomes parent";
            childBecomesItsParent(face1BIdx);
        }
        else
        {
            stats.bottomEdgeCase = "Stem becomes child";
            face1B.interval.x = face2B.interval.x;
            face1B.color = face2B.color;
            auto &parent = mesh.edges[face1B.parentIdx];
            parent.removeChildIdx(face2BIdx);
            parent.removeChildIdx(face2LIdx);
        }
        if (!topIsStem || !alreadyCopied)
            transferChildTo(face2RIdx, face1RIdx);

        bottomEdge = test2 = addBottomT = 0;
    }

    switch (bottomEdge)
    {
    case LeftT | RightT:
    {
        // 0 -> 20 -> 14
        stats.bottomEdgeCase = "LeftT and RightT";
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
        // 1 -> 31 -> 9 (now i don't have to twist my head)
        // the bottom left edge is not a stem.. its twin is a stem
        // the bottom left edge inherits the bar data from the bottom right edge
        stats.bottomEdgeCase = "RightT (w/ or w/o LeftL)";
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
        test2 = false;
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
        stats.bottomEdgeCase = "LeftT (w/ or w/o RightL)";
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
        stats.bottomEdgeCase = "RightL (w/ or w/o LeftL)";
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        transferChildTo(face2BIdx, face1BIdx);
        break;
    }
    default:
    {
        break;
    }
    }

    if (test)
    {
        assert(topLeftEdge->handleIdxs.first != -1 && topLeftEdge->handleIdxs.second != -1);
        assert(topRightEdge->handleIdxs.second != -1);

        float topLeftScale = 1.0f / topEdgeT;
        float topRightScale = 1.0f / (1.0f - topEdgeT);
        auto &topLeftHandle = mesh.handles[topLeftEdge->handleIdxs.first];
        auto &topRightHandle = mesh.handles[topLeftEdge->handleIdxs.second];
        topLeftHandle *= topLeftScale;
        topRightHandle = mesh.handles[topRightEdge->handleIdxs.second] * topRightScale;
        face1R.copyGeometricData(face2R);
    }

    if (test2)
    {
        assert(bottomLeftEdge->handleIdxs.first != -1 && bottomLeftEdge->handleIdxs.second != -1);
        assert(bottomRightEdge->handleIdxs.first != -1);

        float bottomLeftScale = 1.0f / bottomEdgeT;
        float bottomRightScale = 1.0f / (1.0f - bottomEdgeT);
        auto &bottomLeftHandle = mesh.handles[bottomLeftEdge->handleIdxs.second];
        auto &bottomRightHandle = mesh.handles[bottomLeftEdge->handleIdxs.first];
        bottomLeftHandle *= bottomLeftScale;
        bottomRightHandle = mesh.handles[bottomRightEdge->handleIdxs.first] * bottomRightScale;
        bottomLeftEdge->copyGeometricData(*bottomRightEdge);
    }

    face1R.handleIdxs = face2R.handleIdxs;
    setNextRightL(face2R, face1BIdx);

    // appState.updateMergeInfo(mergeEdgeIdx, t, face2L.faceIdx, 1.0f - topEdgeT, bottomEdgeT);
    int topTwinIdx = addTopT ? addTJunction(*topRightEdge, *topLeftEdge, newTopEdgeIdx, 1.0f - topEdgeT) : -1;
    int bottomTwinIdx = addBottomT ? addTJunction(*bottomLeftEdge, *bottomRightEdge, newBottomEdgeIdx, bottomEdgeT) : -1;

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
    const auto &leftPv01 = mesh.computeEdgeDerivatives(bar1)[2].coords;  // left patch: P_v(0,1)
    const auto &rightPv00 = mesh.computeEdgeDerivatives(bar2)[1].coords; // right patch: P_v(0,0)
    const auto &leftPuv01 = stem.twist.coords;                           // left patch: P_uv(0,1)
    const auto &rightPuv00 = bar2.twist.coords;                          // right patch: P_uv(0,0)

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
float GradMeshMerger::addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t)
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

    auto &bar1 = mesh.edges[bar1Idx];
    auto &bar2 = mesh.edges[bar2Idx];

    int stemIdx = bar1.nextIdx;
    if (mesh.edges[stemIdx].isBar())
        stemIdx = mesh.edges[bar1.nextIdx].parentIdx;

    if (bar2.isParent() && bar1.isParent())
    {
        // strategy: remove the parent of bar2 and update bar1
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        scaleUpChildrenByT(bar2, t);
        bar1.addChildrenIdxs(bar2.childrenIdxs);
        setChildrenNewParent(bar2, parentIdx);
        bar2.disable();
    }
    else if (bar1.isParent())
    {
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        bar1.addChildrenIdxs({bar2Idx});
        bar2.createBar(parentIdx, {t, 1});
    }
    else if (bar2.isParent())
    {
        parentIdx = bar2Idx;
        scaleUpChildrenByT(bar2, t);
        bar2.addChildrenIdxs({bar1Idx});
    }
    else
    {
        parentIdx = mesh.addEdge(HalfEdge{});
        mesh.edges[parentIdx].childrenIdxs = {bar1Idx, bar2Idx};
        bar2.createBar(parentIdx, {t, 1});
    }

    if (!bar1.isParent())
    {
        if (bar1.isStem())
        {
            // 11 --> 40 -- > 23
            transferChildTo(bar1Idx, parentIdx);
            mesh.edges[parentIdx].handleIdxs = bar1.handleIdxs;
        }
        mesh.edges[parentIdx].copyGeometricData(bar1);
        bar1.createBar(parentIdx, {0, t});
    }

    mesh.edges[stemIdx].createStem(parentIdx, {t, t});
    auto &parent = mesh.edges[parentIdx];
    parent.handleIdxs = {twinHandles.second, twinHandles.first};
    parent.twinIdx = twinOfParentIdx;

    parent.nextIdx = bar2.nextIdx;

    parent.addChildrenIdxs({stemIdx});
    setBarChildrensTwin(parent, twinOfParentIdx);

    // mesh.edges[twinOfParentIdx].twinIdx = parentIdx;
    setParentChildrenTwin(mesh.edges[twinOfParentIdx], parentIdx);

    return t;
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
    std::cout << parentIdx << "\n";
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
    std::cout << "parent is: " << bar.parentIdx;
    auto &parent = mesh.edges[bar.parentIdx];
    auto &twin = mesh.edges[parent.twinIdx];
    if (bar.twinIdx != parent.twinIdx)
    {
        std::cout << "fixing the twin: prev: " << bar.twinIdx << " new: " << parent.twinIdx << "\n";
        bar.twinIdx = parent.twinIdx;
    }

    twin.twinIdx = barIdx;
    std::cout << "setting " << bar.twinIdx << " to " << mesh.edges[bar.twinIdx].twinIdx << ".\n";
}

bool GradMeshMerger::twinFaceIsRightCycle(const HalfEdge &e) const
{
    if (e.twinIdx == -1)
        return false;

    int edgeIdx = e.twinIdx;
    int rightCycle = 0;

    for (int i = 0; i < 4; ++i)
    {
        auto &e1 = mesh.edges[edgeIdx];
        if (e1.isRightMostChild())
            ++rightCycle;
        edgeIdx = e1.nextIdx;
    }

    return rightCycle >= 3;
}

bool GradMeshMerger::twinFaceIsCycle(const HalfEdge &e, const HalfEdge &adjEdge) const
{
    if (e.twinIdx == -1)
        return false;

    auto &adjTwinEdge = mesh.edges[adjEdge.twinIdx];
    std::vector<int> adjBars = {};
    // auto adjBars = mesh.getBarChildren(adjTwinEdge);
    adjBars.push_back(adjEdge.twinIdx);

    auto &twinEdge = mesh.edges[e.twinIdx];

    auto barChildrenIdxs = mesh.getBarChildren(twinEdge);
    if (barChildrenIdxs.empty())
        barChildrenIdxs.push_back(e.twinIdx);

    std::vector<int> stemParents;

    for (int barChildIdx : barChildrenIdxs)
    {
        auto &nextEdge = mesh.edges[mesh.edges[barChildIdx].nextIdx];
        if (nextEdge.isBar())
        {
            auto &e2parent = mesh.edges[nextEdge.parentIdx];
            auto newSP = mesh.getStemParentChildren(e2parent);
            stemParents.insert(stemParents.end(), newSP.begin(), newSP.end());
        }
    }

    if (stemParents.empty())
        return false;

    std::vector<int> stemParents2;
    for (int edgeIdx : stemParents)
    {
        auto &stemParent = mesh.edges[edgeIdx];
        auto newSP = mesh.getStemParentChildren(stemParent);
        stemParents2.insert(stemParents2.end(), newSP.begin(), newSP.end());
    }
    if (stemParents2.empty())
        return false;

    for (int edgeIdx : stemParents2)
    {
        auto &stemParent = mesh.edges[edgeIdx];
        for (int childIdx : stemParent.childrenIdxs)
        {
            auto &child = mesh.edges[childIdx];
            if (child.isStem())
            {
                if (childIdx == e.twinIdx || std::ranges::find(adjBars, childIdx) != adjBars.end())
                    return true;
            }
        }
    }
    return false;
}

bool GradMeshMerger::twinFaceIsCCWCycle(const HalfEdge &e) const
{
    if (e.twinIdx == -1)
        return false;

    auto &twinEdge = mesh.edges[e.twinIdx];
    std::vector<int> barChildrenIdxs;
    std::vector<int> stemParents;

    if (twinEdge.isParent())
    {
        for (int childIdx : twinEdge.childrenIdxs)
            if (mesh.edges[childIdx].isBar())
                barChildrenIdxs.push_back(childIdx);
    }
    else
        barChildrenIdxs.push_back(e.twinIdx);

    for (int idx : barChildrenIdxs)
    {
        if (mesh.edges[idx].isBar())
        {
            auto &e1parent = mesh.edges[mesh.edges[idx].parentIdx];
            auto newSP = mesh.getStemParentChildren(e1parent);
            stemParents.insert(stemParents.end(), newSP.begin(), newSP.end());
        }
    }

    if (stemParents.empty())
        return false;

    std::vector<int> stemParents2;
    for (int edgeIdx : stemParents)
    {
        auto &stemParent = mesh.edges[edgeIdx];
        auto newSP = mesh.getStemParentChildren(stemParent);
        stemParents2.insert(stemParents2.end(), newSP.begin(), newSP.end());
    }
    if (stemParents2.empty())
        return false;

    for (int edgeIdx : stemParents2)
    {
        auto &stemParent = mesh.edges[edgeIdx];
        for (int childIdx : stemParent.childrenIdxs)
        {
            auto &child = mesh.edges[childIdx];
            if (child.isStem())
            {
                if (child.twinIdx == e.nextIdx)
                    return true;
            }
        }
    }
    return false;
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
