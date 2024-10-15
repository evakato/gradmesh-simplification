#include "merging.hpp"

GradMeshMerger::GradMeshMerger(GradMesh &mesh, GmsAppState &appState) : mesh(mesh), appState(appState)
{
    findCandidateMerges();
    appState.numOfCandidateMerges = candidateMerges.size();
}

bool GradMeshMerger::mergeAtSelectedEdge()
{
    if (candidateMerges.empty() || appState.selectedEdgeId < 0 || appState.selectedEdgeId >= candidateMerges.size())
        return false;

    int halfEdgeIdx = candidateMerges[appState.selectedEdgeId].getHalfEdgeIdx();
    mergePatches(halfEdgeIdx);
    findCandidateMerges();
    return true;
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
                if (currEdge.hasTwin() && !currEdge.isBar() && !mesh.twinIsParent(currEdge) && !currEdge.isParent())
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
                currIdx = currEdge.nextIdx;
            }
            faceIdx++;
        }
    }

    for (auto &doublehe : candidateMerges)
    {
        // std::cout << doublehe.halfEdgeIdx1 << ", " << doublehe.halfEdgeIdx2 << "\n";
    }
}

void GradMeshMerger::mergePatches(int mergeEdgeIdx)
{
    std::cout << "Merging: " << mergeEdgeIdx << "\n";
    writeLogFile(mesh, appState, "debug1.txt");

    int face1RIdx = mergeEdgeIdx;
    auto &face1R = mesh.edges[face1RIdx];
    int face1BIdx = face1R.nextIdx;
    auto &face1B = mesh.edges[face1BIdx];
    int face1LIdx = face1B.nextIdx;
    auto &face1L = mesh.edges[face1LIdx];
    int face1TIdx = face1L.nextIdx;
    auto &face1T = mesh.edges[face1TIdx];

    int face2LIdx = face1R.twinIdx;
    auto &face2L = mesh.edges[face2LIdx];
    int face2TIdx = face2L.nextIdx;
    auto &face2T = mesh.edges[face2TIdx];
    int face2RIdx = face2T.nextIdx;
    auto &face2R = mesh.edges[face2RIdx];
    int face2BIdx = face2R.nextIdx;
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
    bool topLeftT = face1T.isBar() && mesh.twinIsStem(face1L);
    bool topRightL = face2R.isBar() && mesh.twinIsStem(face2T);
    bool topRightT = face2T.isBar() && (face2R.isStem() || mesh.parentIsStem(face2R));
    bool topIsStem = face1R.isStem();
    int topEdge = getCornerTJunctions(topLeftL, topLeftT, topRightL, topRightT);

    bool bottomLeftL = face1L.isBar() && mesh.twinIsStem(face1B);
    bool bottomLeftT = (face1L.isStem() || mesh.parentIsStem(face1L)) && face1B.isBar();
    bool bottomRightL = face2R.isBar() && face2B.isStem();
    bool bottomRightT = face2B.isBar() && mesh.twinIsStem(face2R);
    bool bottomIsStem = face2L.isStem();
    int bottomEdge = getCornerTJunctions(bottomLeftL, bottomLeftT, bottomRightL, bottomRightT);

    std::cout << bottomLeftEdge->parentIdx << "\n";
    std::cout << bottomLeftEdge->isBar() << "\n";

    float topEdgeT = topIsStem ? 0 : splittingFactor(face1R, face1T, face2T, 1);
    float bottomEdgeT = bottomIsStem ? 0 : splittingFactor(face2L, face1B, face2B, 1);
    float t = (topIsStem && bottomIsStem) ? 0 : (topEdgeT + bottomEdgeT) / (!topIsStem + !bottomIsStem);

    mesh.disablePoint(face1R.originIdx);
    mesh.disablePoint(face1B.originIdx);

    appState.t = t;
    appState.removedFaceId = face2L.faceIdx;
    appState.bottomEdgeCase = "None";
    appState.topEdgeCase = "None";

    if (topIsStem)
    {
        appState.topEdgeCase = "Stem";
        transferChildTo(face2RIdx, face1RIdx);
        if (face1T.isLeftMostChild() && face2T.isRightMostChild())
        {
            childBecomesItsParent(face1TIdx);
            std::cout << "wth" << std::endl;
        }
        else
        {
            face1T.interval.y = face2T.interval.y;
            setNextRightL(face2T, face1RIdx);
            auto &parent = mesh.edges[face1T.parentIdx];
            parent.removeChildIdx(face1RIdx);
            parent.removeChildIdx(face2TIdx);
        }
        topEdge = test = addTopT = 0;
    }
    switch (topEdge)
    {
    case LeftT | RightT:
    {
        // 9 --> 18 --> 8
        appState.topEdgeCase = "LeftT and RightT";
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
        appState.topEdgeCase = "RightT (w/ or w/o LeftL)";
        newTopEdgeIdx = face2T.parentIdx;
        auto topRightEdge = &mesh.edges[newTopEdgeIdx];

        auto [newCurvePart, totalCurve] = parameterizeTBar2(t / (1.0f - t), face2T);
        topEdgeT = newCurvePart / totalCurve;

        mesh.handles[topRightEdge->handleIdxs.first] = mesh.handles[face1T.handleIdxs.first] * (1.0f / topEdgeT);
        mesh.handles[topRightEdge->handleIdxs.second] *= (1.0f / (1.0f - topEdgeT));

        topRightEdge->copyGeometricData(face1T);
        transferChildTo(face2RIdx, face1RIdx);
        face1T.createBar(-1, {0, 1});
        transferChildToWithoutGeometry(face2TIdx, face1TIdx);
        rightTUpdateInterval(newTopEdgeIdx, newCurvePart, totalCurve);
        test = false;
        break;
    }
    case LeftT | RightL:
        // 9 --> 47 --> 8 or 16 -> 38 --> 26
        transferChildToWithoutGeometry(face2TIdx, face1T.parentIdx);
        transferChildTo(face2RIdx, face1RIdx);
        // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
        [[fallthrough]];
    case LeftT:
    {
        appState.topEdgeCase = "LeftT (w/ or w/o RightL)";
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
        appState.topEdgeCase = "RightL (w/ or w/o LeftL)";
        // for leftL, weird case: basically just delete the stem from the RightL
        // 2 --> 49 --> 7 or 18 --> 35 --> 16
        // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        break;
    }
    case LeftL:
    {
        appState.topEdgeCase = "LeftL only";
    }
    default:
    {
        break;
    }
    }

    if (bottomIsStem)
    {
        appState.bottomEdgeCase = "Stem";
        // transferChildTo(face2RIdx, face1RIdx);
        if (face2B.isLeftMostChild() && face1B.isRightMostChild())
        {
            childBecomesItsParent(face1BIdx);
            std::cout << "wth2" << std::endl;
        }
        else
        {
            face1B.interval.x = face2B.interval.x;
            face1B.color = face2B.color;
            auto &parent = mesh.edges[face1B.parentIdx];
            parent.removeChildIdx(face2BIdx);
            parent.removeChildIdx(face2LIdx);
        }
        bottomEdge = test2 = addBottomT = 0;
    }

    switch (bottomEdge)
    {
    case LeftT | RightT:
    {
        // 0 -> 20 -> 14
        appState.bottomEdgeCase = "LeftT and RightT";
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
        bottomRightEdge->disable();
        break;
    }
    case LeftL | RightT:
    case RightT:
    {
        // 1 -> 31 -> 9 (now i don't have to twist my head)
        // the bottom left edge is not a stem.. its twin is a stem
        // the bottom left edge inherits the bar data from the bottom right edge
        appState.bottomEdgeCase = "RightT (w/ or w/o LeftL)";
        newBottomEdgeIdx = face2B.parentIdx;
        bottomRightEdge = &mesh.edges[newBottomEdgeIdx];

        auto [newCurvePart, totalCurve] = parameterizeTBar1(t / (1.0f - t), face2B);
        bottomEdgeT = newCurvePart / totalCurve;

        mesh.handles[bottomRightEdge->handleIdxs.first] *= (1.0f / (1.0f - bottomEdgeT));
        mesh.handles[bottomRightEdge->handleIdxs.second] = mesh.handles[face1B.handleIdxs.second] * (1.0f / bottomEdgeT);
        bottomRightEdge->nextIdx = face1LIdx; // important !!!

        // mesh.copyEdge(face1BIdx, face2BIdx);
        face1B.createBar(-1, {0, 1});
        transferChildTo(face2BIdx, face1BIdx);
        // mesh.edges[newBottomEdgeIdx].replaceChild(face2BIdx, face1BIdx);

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
        appState.bottomEdgeCase = "LeftT (w/ or w/o RightL)";
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
    case LeftL | RightL:
    case RightL:
    {
        // with LeftL: 2 --> 49 --> 16
        appState.bottomEdgeCase = "RightL (w/ or w/o LeftL)";
        transferChildToWithoutGeometry(face2RIdx, face1RIdx);
        transferChildToWithoutGeometry(face2BIdx, face1BIdx);
        break;
    }
    case LeftL:
    {
        appState.bottomEdgeCase = "LeftL only";
    }
    default:
    {
        break;
    }
    }

    saveImage((std::string{LOGS_DIR} + "/" + extractFileName(appState.filename) + ".png").c_str(), GL_LENGTH, GL_LENGTH);
    writeLogFile(mesh, appState, "debug2.txt");

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

    appState.topEdgeT = addTopT ? addTJunction(*topRightEdge, *topLeftEdge, newTopEdgeIdx, 1.0f - topEdgeT) : 0;
    appState.bottomEdgeT = addBottomT ? addTJunction(*bottomLeftEdge, *bottomRightEdge, newBottomEdgeIdx, bottomEdgeT) : 0;

    copyEdgeTwin(face1RIdx, face2RIdx);
    removeFace(face2L.faceIdx);
}

const float GradMeshMerger::splittingFactor(HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign) const
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

    int bar1Idx = edge1.twinIdx;
    int bar2Idx = edge2.twinIdx;
    auto &bar1 = mesh.edges[bar1Idx];
    auto &bar2 = mesh.edges[bar2Idx];
    auto twinHandles = mesh.edges[twinOfParentIdx].handleIdxs;
    int parentIdx;
    int stemIdx = bar1.nextIdx;
    if (mesh.edges[stemIdx].isBar())
        stemIdx = mesh.edges[bar1.nextIdx].parentIdx;

    if (bar2.isParent() && bar1.isParent())
    {
        // strategy: remove the parent of bar2 and update bar1
        std::cout << "Both bars are parents\n";
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        scaleUpChildrenByT(bar2, t);
        bar1.addChildrenIdxs(bar2.childrenIdxs);
        setChildrenNewParent(bar2, parentIdx);
        bar2.disable();
    }
    else if (bar1.isParent())
    {
        std::cout << "bar1 is a parent\n";
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        bar1.addChildrenIdxs({bar2Idx});
        bar2.createBar(parentIdx, {t, 1});
    }
    else if (bar2.isParent())
    {
        std::cout << "bar2 is a parent\n";
        parentIdx = bar2Idx;
        scaleUpChildrenByT(bar2, t);
        bar2.addChildrenIdxs({bar1Idx});
    }
    else
    {
        std::cout << "neither is a parent\n";
        parentIdx = mesh.addEdge(HalfEdge{});
        mesh.edges[parentIdx].childrenIdxs = {bar1Idx, bar2Idx};
        bar2.createBar(parentIdx, {t, 1});
    }

    if (!bar1.isParent())
    {
        if (bar1.isStem())
        {
            // 11 --> 40 -- > 23
            std::cout << "Extending stem!\n";
            // copyAndReplaceChild(parentIdx, bar1Idx);
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
    mesh.edges[twinOfParentIdx].twinIdx = parentIdx;

    return t;
}

void GradMeshMerger::setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx)
{
    for (int childIdx : parentEdge.childrenIdxs)
        mesh.edges[childIdx].parentIdx = newParentIdx;
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

bool GradMeshMerger::twinFaceIsCycle(const HalfEdge &e) const
{
    if (e.twinIdx == -1)
        return false;

    auto &e1 = mesh.edges[e.twinIdx];
    int count = 0;

    if (e1.isChild() || e1.isParent())
        ++count;
    auto &e2 = mesh.edges[e1.nextIdx];
    if (e2.isChild() || e2.isParent())
        ++count;
    auto &e3 = mesh.edges[e2.nextIdx];
    if (e3.isChild() || e3.isParent())
        ++count;
    auto &e4 = mesh.edges[e3.nextIdx];
    if (e4.isChild() || e4.isParent())
        ++count;

    return count >= 3;
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
