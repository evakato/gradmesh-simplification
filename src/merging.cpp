#include "gradmesh.hpp"

namespace Merging
{
    bool mergeAtSelectedEdge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, GmsAppState &appState)
    {
        if (candidateMerges.empty() || appState.selectedEdgeId < 0 || appState.selectedEdgeId >= candidateMerges.size())
            return false;

        int halfEdgeIdx = candidateMerges[appState.selectedEdgeId].getHalfEdgeIdx();
        mergePatches(mesh, halfEdgeIdx, appState);
        candidateMerges = candidateEdges(mesh);
        return true;
    }

    std::vector<DoubleHalfEdge> candidateEdges(const GradMesh &mesh)
    {
        std::vector<DoubleHalfEdge> edgeList = {};
        int faceIdx = 0;
        for (auto &face : mesh.getFaces())
        {
            if (face.isValid())
            {
                int currIdx = face.halfEdgeIdx;
                for (int i = 0; i < 4; i++)
                {
                    const auto &currEdge = mesh.getEdge(currIdx);
                    if (currEdge.hasTwin() && !currEdge.isBar() && !mesh.twinIsTJunction(currEdge) && !currEdge.isParent())
                    {
                        auto findEdgeIdx = std::find_if(edgeList.begin(), edgeList.end(), [currIdx](const DoubleHalfEdge &dhe)
                                                        { return dhe.halfEdgeIdx2 == currIdx; });

                        if (findEdgeIdx == edgeList.end())
                        {
                            edgeList.push_back(DoubleHalfEdge{currIdx, currEdge.twinIdx, CurveId{faceIdx, i}});
                        }
                        else
                        {
                            size_t foundIndex = std::distance(edgeList.begin(), findEdgeIdx);
                            // assert(currIdx != edgeList[foundIndex].halfEdgeIdx2)
                            edgeList[foundIndex]
                                .curveId2 = CurveId{faceIdx, i};
                        }
                    }
                    currIdx = currEdge.nextIdx;
                }
                faceIdx++;
            }
        }

        for (auto &doublehe : edgeList)
        {
            // std::cout << doublehe.halfEdgeIdx1 << ", " << doublehe.halfEdgeIdx2 << "\n";
        }
        return edgeList;
    }

    void mergePatches(GradMesh &mesh, int mergeEdgeIdx, GmsAppState &appState)
    {
        std::cout << "Merging: " << mergeEdgeIdx << "\n";
        writeLogFile(mesh, appState, "debug1.txt");

        int face1e1Idx = mergeEdgeIdx;
        auto &face1e1 = mesh.edges[face1e1Idx];
        int face1e2Idx = face1e1.nextIdx;
        auto &face1e2 = mesh.edges[face1e2Idx];
        int face1e3Idx = face1e2.nextIdx;
        auto &face1e3 = mesh.edges[face1e3Idx];
        int face1e4Idx = face1e3.nextIdx;
        auto &face1e4 = mesh.edges[face1e4Idx];

        int face2e1Idx = face1e1.twinIdx;
        auto &face2e1 = mesh.edges[face2e1Idx];
        int face2e2Idx = face2e1.nextIdx;
        auto &face2e2 = mesh.edges[face2e2Idx];
        int face2e3Idx = face2e2.nextIdx;
        auto &face2e3 = mesh.edges[face2e3Idx];
        int face2e4Idx = face2e3.nextIdx;
        auto &face2e4 = mesh.edges[face2e4Idx];

        auto *topLeftEdge = &face1e4;
        auto *topRightEdge = &face2e2;
        auto *bottomLeftEdge = &face1e2;
        auto *bottomRightEdge = &face2e4;
        int newTopEdgeIdx = face1e4Idx;
        int newBottomEdgeIdx = face1e2Idx;

        // ya bitch can rly name shit well
        bool test = true;
        bool test2 = true;
        bool addTopTJunction = true;
        bool addBottomTJunction = true;

        bool topLeftL = face1e3.isBar() && face1e4.isStem();
        bool topLeftT = face1e4.isBar() && mesh.twinIsStem(face1e3);
        bool topRightL = face2e3.isBar() && mesh.twinIsStem(face2e2);
        bool topRightT = face2e2.isBar() && face2e3.isStem();
        bool topIsStem = face1e1.isStem();
        int topEdge = getCornerFlags(topLeftL, topLeftT, topRightL, topRightT);

        bool bottomLeftL = face1e3.isBar() && mesh.twinIsStem(face1e2);
        bool bottomLeftT = face1e3.isStem() && face1e2.isBar();
        bool bottomRightL = face2e3.isBar() && face2e4.isStem();
        bool bottomRightT = face2e4.isBar() && mesh.twinIsStem(face2e3);
        bool bottomIsStem = face2e1.isStem();
        int bottomEdge = getCornerFlags(bottomLeftL, bottomLeftT, bottomRightL, bottomRightT);

        appState.removedFaceId = face2e1.faceIdx;

        if (mesh.twinFaceIsCycle(face1e4))
            return;
        if (mesh.twinFaceIsCycle(face2e2))
            return;
        if (mesh.twinFaceIsCycle(face1e2))
            return;
        if (mesh.twinFaceIsCycle(face2e4))
            return;

        if (topIsStem && bottomIsStem)
        {
            // the problemo is that i'm not updating the twin idxs at all :)
            auto &parentTop = mesh.edges[face1e4.parentIdx];
            if (face1e4.interval.x == 0 && face2e2.interval.y == 1)
            {
                mesh.fixAndSetTwin(face1e4Idx);
                mesh.copyEdge(face1e4Idx, face1e4.parentIdx);
                parentTop.disable();
            }
            else
            {
                face1e4.interval.y = face2e2.interval.y;
            }
            mesh.copyEdge(face1e1Idx, face2e3Idx);
            if (face2e4.interval.x == 0 && face1e2.interval.y == 1)
            {
                auto &parent = mesh.edges[face2e4.parentIdx];
                mesh.fixAndSetTwin(face1e2Idx);
                mesh.copyEdge(face1e2Idx, face2e4.parentIdx);
                parent.disable();
            }
            else
            {
                face1e2.interval.x = face2e4.interval.x;
                face1e2.copyGeometricData(face2e4);
            }

            if (face2e2.interval.y == 1)
                parentTop.nextIdx = face1e1Idx;

            mesh.copyEdgeTwin(face1e1Idx, face2e3Idx);
            mesh.removeFace(face2e1.faceIdx);
            appState.t = -1;
            return;
        }

        float t = 0;
        float topEdgeT = 0;
        float bottomEdgeT = 0;
        int tApproxCount = 0;
        if (!topIsStem)
        {
            topEdgeT = splittingFactor(mesh, face1e1, face1e4, face2e2, 1);
            tApproxCount++;
        }
        if (!bottomIsStem)
        {
            bottomEdgeT = splittingFactor(mesh, face2e1, face1e2, face2e4, 1);
            tApproxCount++;
        }
        t = topEdgeT + bottomEdgeT;
        if (tApproxCount != 0)
            t /= tApproxCount;

        appState.t = t;

        if (topIsStem)
        {
            appState.topEdgeCase = "Stem";
            topEdge = -1;
            face1e1.copyGeometricData(face2e3);
            test = false;
            auto &parent = mesh.edges[face1e4.parentIdx];
            if (face1e4.interval.x == 0 && face2e2.interval.y == 1)
            {
                std::cout << "converting to normal!\n";
                mesh.fixAndSetTwin(face1e4Idx);
                mesh.copyEdge(face1e4Idx, face1e4.parentIdx);
                parent.disable();
            }
            else
            {
                std::cout << "failing to normal!\n";
                face1e4.interval.y = face2e2.interval.y;
                if (face2e2.interval.y == 1)
                    parent.nextIdx = face1e1Idx;
            }
            mesh.copyChild(face1e1Idx, face2e3Idx);
            if (face2e3.interval.y == 1)
                mesh.edges[face2e3.parentIdx].nextIdx = face1e2Idx;
            addTopTJunction = false;
        }

        switch (topEdge)
        {
        case LeftT | RightT:
        {
            // 9 --> 18 --> 8
            appState.topEdgeCase = "LeftT and RightT";
            newTopEdgeIdx = face1e4.parentIdx;
            topLeftEdge = &mesh.edges[newTopEdgeIdx];
            int rightTParentIdx = face2e2.parentIdx;
            topRightEdge = &mesh.edges[rightTParentIdx];

            float totalRelativeLeft = totalCurveRelativeLeft((1.0f - t) / t, face1e4, face2e2);
            float totalRelativeRight = totalCurveRelativeRight(t / (1.0f - t), face2e2, face1e4);
            topEdgeT = 1.0f / totalRelativeLeft;

            // reparameterize the intervals for the right T children
            for (int childIdx : topRightEdge->childrenIdxs)
            {
                mesh.edges[childIdx].parentIdx = newTopEdgeIdx;
                mesh.edges[childIdx].reparamInterval(totalRelativeRight - 1.0f, totalRelativeRight);
            }

            // reparameterize the intervals for the left T children
            for (int childIdx : topLeftEdge->childrenIdxs)
            {
                mesh.edges[childIdx].interval /= totalRelativeLeft;
            }
            // left T inherits the children from right T
            mesh.edges[rightTParentIdx].replaceChild(face2e2Idx, face1e4Idx);
            mesh.edges[rightTParentIdx].replaceChild(face2e3Idx, face1e1Idx);
            topLeftEdge->addChildrenIdxs(topRightEdge->childrenIdxs);
            topLeftEdge->nextIdx = topRightEdge->nextIdx;

            // update new children and disable right T parent
            mesh.copyChild(face1e1Idx, face2e3Idx);
            face1e4.interval.y = face2e2.interval.y;
            topRightEdge->disable();
            break;
        }
        case LeftL | RightT:
            // if top left edge is a stem, turn the parent into that stem
            // 2 --> 20 --> 7 (no twisting head) or 3 --> 24 --> 36
            mesh.copyChild(face2e2.parentIdx, face1e4Idx);
            [[fallthrough]];
        case RightT:
        {
            // 41 --> 23
            // keep the parent, make the top left edge the bar1
            appState.topEdgeCase = "RightT (w/ or w/o LeftL)";
            newTopEdgeIdx = face2e2.parentIdx;
            auto topRightEdge = &mesh.edges[newTopEdgeIdx];

            auto [newCurvePart, totalCurve] = parameterizeTBar2(t / (1.0f - t), face2e2);
            topEdgeT = newCurvePart / totalCurve;

            mesh.getHandle(topRightEdge->handleIdxs.first) = mesh.getHandle(face1e4.handleIdxs.first) * (1.0f / topEdgeT);
            mesh.getHandle(topRightEdge->handleIdxs.second) *= (1.0f / (1.0f - topEdgeT));

            topRightEdge->copyGeometricDataExceptHandles(face1e4);
            mesh.copyEdge(face1e1Idx, face2e3Idx);
            mesh.copyChild(face1e4Idx, face2e2Idx);
            face1e4.handleIdxs = face2e2.handleIdxs;
            face1e4.originIdx = -1;

            // mesh.edges[newTopEdgeIdx].replaceChild(face2e2Idx, face1e4Idx);
            // mesh.edges[newTopEdgeIdx].replaceChild(face2e3Idx, face1e1Idx);

            for (int childIdx : topRightEdge->childrenIdxs)
            {
                mesh.edges[childIdx].reparamInterval(newCurvePart, totalCurve);
            }
            face1e4.interval.x = 0;

            test = false;
            break;
        }
        case LeftT | RightL:
            // 9 --> 47 --> 8 or 16 -> 38 --> 26
            mesh.copyChild(face1e4.parentIdx, face2e2Idx);
            mesh.copyChild(face1e1Idx, face2e3Idx);
            mesh.edges[face2e3.parentIdx].nextIdx = face1e2Idx;
            [[fallthrough]];
        case LeftT:
        {
            appState.topEdgeCase = "LeftT (w/ or w/o RightL)";
            newTopEdgeIdx = face1e4.parentIdx;
            topLeftEdge = &mesh.edges[newTopEdgeIdx];
            auto [_, totalCurve] = parameterizeTBar1((1.0f - t) / t, face1e4);
            topEdgeT = 1.0f / totalCurve;
            mesh.leftTUpdateInterval(newTopEdgeIdx, totalCurve);
            break;
        }
        case LeftL | RightL:
        case RightL:
        {
            appState.topEdgeCase = "RightL (w/ or w/o LeftL)";
            // for leftL, weird case: basically just delete the stem from the RightL
            // 2 --> 49 --> 7 or 18 --> 35 --> 16
            mesh.edges[face2e3.parentIdx].nextIdx = face1e2Idx;
            mesh.copyChild(face1e1Idx, face2e3Idx);
            // mesh.edges[face2e3.parentIdx].replaceChild(face2e3Idx, face1e1Idx);
            break;
        }
        case LeftL:
        {
            appState.topEdgeCase = "LeftL only";
        }
        case None:
        {
            appState.topEdgeCase = "None";
            break;
        }
        default:
        {
            break;
        }
        }

        if (bottomIsStem)
        {
            appState.bottomEdgeCase = "Stem";
            bottomEdge = -1;
            auto &parent = mesh.edges[face1e2.parentIdx];
            if (parent.isChild())
            {
                mesh.copyChild(face1e1Idx, face2e3Idx);
            }
            test2 = false;
            if (face2e4.interval.x == 0 && face1e2.interval.y == 1)
            {
                mesh.copyEdge(face1e1Idx, face2e3Idx);
                mesh.fixAndSetTwin(face1e2Idx);
                mesh.copyEdge(face1e2Idx, face1e2.parentIdx);
                std::cout << "we fin get busy\n";
                parent.disable();
            }
            else
            {
                face1e2.interval.x = face2e4.interval.x;
                face1e2.color = face2e4.color;
            }
            addBottomTJunction = false;
        }

        switch (bottomEdge)
        {
        case LeftT | RightT:
        {
            // 0 -> 20 -> 14
            appState.bottomEdgeCase = "LeftT and RightT";
            newBottomEdgeIdx = face1e2.parentIdx;
            bottomLeftEdge = &mesh.edges[newBottomEdgeIdx];
            int rightTParentIdx = face2e4.parentIdx;
            bottomRightEdge = &mesh.edges[rightTParentIdx];

            float totalRelativeRight = totalCurveRelativeRight((1.0f - t) / t, face1e2, face2e4);
            float totalRelativeLeft = totalCurveRelativeLeft(t / (1.0f - t), face2e4, face1e2);
            bottomEdgeT = 1.0f / totalRelativeRight;

            // reparameterize the intervals for the right T children
            for (int childIdx : bottomRightEdge->childrenIdxs)
            {
                mesh.edges[childIdx].parentIdx = newBottomEdgeIdx;
                mesh.edges[childIdx].interval /= totalRelativeLeft;
            }
            // reparameterize the intervals for the left T children
            for (int childIdx : bottomLeftEdge->childrenIdxs)
            {
                mesh.edges[childIdx].reparamInterval(totalRelativeRight - 1.0f, totalRelativeRight);
            }
            mesh.edges[rightTParentIdx].replaceChild(face2e4Idx, face1e2Idx);
            bottomLeftEdge->addChildrenIdxs(bottomRightEdge->childrenIdxs);

            // update new children and disable right T parent
            face1e2.interval.x = face2e4.interval.x;
            face1e2.color = face2e4.color;
            bottomRightEdge->disable();
            break;
        }
        case LeftL | RightT:
        case RightT:
        {
            // 3 -> 22 -> 21 or 1 -> 31 -> 9 (now i don't have to twist my head)
            appState.bottomEdgeCase = "RightT (w/ or w/o LeftL)";
            // the bottom left edge is not a stem.. its twin is a stem
            // the bottom left edge inherits the bar data from the bottom right edge
            newBottomEdgeIdx = face2e4.parentIdx;
            bottomRightEdge = &mesh.edges[newBottomEdgeIdx];
            // auto &rightTParent = mesh.edges[rightTParentIdx];

            auto [newCurvePart, totalCurve] = parameterizeTBar1(t / (1.0f - t), face2e4);
            bottomEdgeT = newCurvePart / totalCurve;

            mesh.getHandle(bottomRightEdge->handleIdxs.first) *= (1.0f / (1.0f - bottomEdgeT));
            mesh.getHandle(bottomRightEdge->handleIdxs.second) = mesh.handles[face1e2.handleIdxs.second] * (1.0f / bottomEdgeT);
            bottomRightEdge->nextIdx = face1e3Idx; // important !!!

            mesh.copyEdge(face1e2Idx, face2e4Idx);
            // mesh.edges[newBottomEdgeIdx].replaceChild(face2e4Idx, face1e2Idx);

            mesh.leftTUpdateInterval(newBottomEdgeIdx, totalCurve);
            test2 = false;
            break;
        }
        case LeftT | RightL:
        {
            // 1 -> 24 -> 31 or 0 --> 47 --> 14 or 17 --> 37 --> 23
            mesh.copyChild(face1e1Idx, face2e3Idx);
            mesh.copyChild(face1e2.parentIdx, face2e4Idx);
            [[fallthrough]];
        }
        case LeftT:
        {
            appState.bottomEdgeCase = "LeftT (w/ or w/o RightL)";
            float newCurvePart = (1.0f - t) / t * face1e2.interval.y;
            float totalCurve = 1.0f + newCurvePart;

            newBottomEdgeIdx = face1e2.parentIdx;
            bottomLeftEdge = &mesh.edges[newBottomEdgeIdx];

            face1e2.color = face2e4.color; // ok like why do bars even have color kms

            for (int childIdx : bottomLeftEdge->childrenIdxs)
            {
                mesh.edges[childIdx].reparamInterval(newCurvePart, totalCurve);
            }
            face1e2.interval.x = 0;
            bottomEdgeT = 1.0f / totalCurve;
            break;
        }
        case LeftL | RightL:
        case RightL:
        {
            // with LeftL: 2 --> 49 --> 16
            appState.bottomEdgeCase = "RightL (w/ or w/o LeftL)";
            mesh.copyChild(face1e1Idx, face2e3Idx);
            mesh.copyChild(face1e2Idx, face2e4Idx);
            // mesh.edges[face2e3.parentIdx].replaceChild(face2e3Idx, face1e1Idx);
            // mesh.edges[face2e4.parentIdx].replaceChild(face2e4Idx, face1e2Idx);
            break;
        }
        case LeftL:
        {
            appState.bottomEdgeCase = "LeftL only";
        }
        case None:
        {
            appState.bottomEdgeCase = "None";
            break;
        }
        default:
        {
            break;
        }
        }

        if (test)
        {
            assert(topLeftEdge->handleIdxs.first != -1);
            assert(topLeftEdge->handleIdxs.second != -1);
            assert(topRightEdge->handleIdxs.second != -1);

            float topLeftScale = 1.0f / topEdgeT;
            float topRightScale = 1.0f / (1.0f - topEdgeT);
            auto &topLeftHandle = mesh.handles[topLeftEdge->handleIdxs.first];
            auto &topRightHandle = mesh.handles[topLeftEdge->handleIdxs.second];
            topLeftHandle *= topLeftScale;
            topRightHandle = mesh.handles[topRightEdge->handleIdxs.second] * topRightScale;
            face1e1.copyGeometricData(face2e3);
        }

        if (test2)
        {
            assert(bottomLeftEdge->handleIdxs.first != -1);
            assert(bottomLeftEdge->handleIdxs.second != -1);
            assert(bottomRightEdge->handleIdxs.first != -1);

            float bottomLeftScale = 1.0f / bottomEdgeT;
            float bottomRightScale = 1.0f / (1.0f - bottomEdgeT);
            auto &bottomLeftHandle = mesh.handles[bottomLeftEdge->handleIdxs.second];
            auto &bottomRightHandle = mesh.handles[bottomLeftEdge->handleIdxs.first];
            bottomLeftHandle *= bottomLeftScale;
            bottomRightHandle = mesh.handles[bottomRightEdge->handleIdxs.first] * bottomRightScale;
            bottomLeftEdge->copyGeometricDataExceptHandles(*bottomRightEdge);
        }

        if (addTopTJunction && topRightEdge->hasTwin() && topLeftEdge->hasTwin())
        {
            mesh.addTJunction(*topRightEdge, *topLeftEdge, newTopEdgeIdx, 1.0f - topEdgeT);
            appState.topEdgeTJunction = 1.0f - topEdgeT;
        }
        else
        {
            appState.topEdgeTJunction = 0;
        }

        if (addBottomTJunction && bottomLeftEdge->hasTwin() && bottomRightEdge->hasTwin())
        {
            mesh.addTJunction(*(bottomLeftEdge), *(bottomRightEdge), newBottomEdgeIdx, bottomEdgeT);
            appState.bottomEdgeTJunction = bottomEdgeT;
        }
        else
        {
            appState.bottomEdgeTJunction = 0;
        }

        mesh.copyEdgeTwin(face1e1Idx, face2e3Idx);
        mesh.removeFace(face2e1.faceIdx);

        saveImage((std::string{LOGS_DIR} + "/" + extractFileName(appState.filename) + ".png").c_str(), GL_LENGTH, GL_LENGTH);
        writeLogFile(mesh, appState, "debug2.txt");
    }

    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign)
    {
        const auto &leftPv01 = mesh.getTangent(bar1, 1).coords;  // left patch: P_v(0,1)
        const auto &rightPv00 = mesh.getTangent(bar2, 0).coords; // right patch: P_v(0,0)
        const auto &leftPuv01 = stem.twist.coords;               // left patch: P_uv(0,1)
        const auto &rightPuv00 = bar2.twist.coords;              // right patch: P_uv(0,0)

        auto sumPv0 = absSum(leftPv01, rightPv00); // P_v(0,1) + P_v(0,0)

        float r1 = glm::length(leftPv01) / glm::length(sumPv0);
        float r2 = glm::length(absSum(leftPv01, sign * BCM * leftPuv01)) / glm::length((sumPv0 + sign * BCM * absSum(leftPuv01, rightPuv00)));

        float t = 0.5 * (r1 + r2);
        // std::cout << "r1: " << r1 << " r2: " << r2 << " t: " << t << "\n";
        return t;
    }
}