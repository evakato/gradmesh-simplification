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

        mesh.disablePoint(face1R.originIdx);
        mesh.disablePoint(face1B.originIdx);

        float topEdgeT = topIsStem ? 0 : splittingFactor(mesh, face1R, face1T, face2T, 1);
        float bottomEdgeT = bottomIsStem ? 0 : splittingFactor(mesh, face2L, face1B, face2B, 1);
        float t = (topIsStem && bottomIsStem) ? 0 : (topEdgeT + bottomEdgeT) / (!topIsStem + !bottomIsStem);

        appState.t = t;
        appState.removedFaceId = face2L.faceIdx;
        appState.bottomEdgeCase = "None";
        appState.topEdgeCase = "None";

        if (topIsStem)
        {
            appState.topEdgeCase = "Stem";
            mesh.transferChildTo(face2RIdx, face1RIdx);
            if (face1T.isLeftMostChild() && face2T.isRightMostChild())
            {
                mesh.childBecomesItsParent(face1TIdx);
                std::cout << "wth" << std::endl;
            }
            else
            {
                face1T.interval.y = face2T.interval.y;
                mesh.setNextRightL(face2T, face1RIdx);
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

            mesh.rightTUpdateInterval(face2T.parentIdx, totalRelativeRight - 1.0f, totalRelativeRight);
            mesh.leftTUpdateInterval(newTopEdgeIdx, totalRelativeLeft);
            mesh.setChildrenNewParent(*topRightEdge, newTopEdgeIdx);

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
            mesh.transferChildTo(face1TIdx, face2T.parentIdx);
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

            mesh.getHandle(topRightEdge->handleIdxs.first) = mesh.getHandle(face1T.handleIdxs.first) * (1.0f / topEdgeT);
            mesh.getHandle(topRightEdge->handleIdxs.second) *= (1.0f / (1.0f - topEdgeT));

            topRightEdge->copyGeometricData(face1T);
            mesh.transferChildTo(face2RIdx, face1RIdx);
            face1T.createBar(-1, {0, 1});
            mesh.transferChildToWithoutGeometry(face2TIdx, face1TIdx);
            mesh.rightTUpdateInterval(newTopEdgeIdx, newCurvePart, totalCurve);
            test = false;
            break;
        }
        case LeftT | RightL:
            // 9 --> 47 --> 8 or 16 -> 38 --> 26
            mesh.transferChildToWithoutGeometry(face2TIdx, face1T.parentIdx);
            mesh.transferChildTo(face2RIdx, face1RIdx);
            // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
            [[fallthrough]];
        case LeftT:
        {
            appState.topEdgeCase = "LeftT (w/ or w/o RightL)";
            newTopEdgeIdx = face1T.parentIdx;
            topLeftEdge = &mesh.edges[newTopEdgeIdx];

            auto [_, totalCurve] = parameterizeTBar1((1.0f - t) / t, face1T);
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
            // mesh.edges[face2R.parentIdx].nextIdx = face1BIdx;
            mesh.transferChildToWithoutGeometry(face2RIdx, face1RIdx);
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
            // mesh.transferChildTo(face2RIdx, face1RIdx);
            if (face2B.isLeftMostChild() && face1B.isRightMostChild())
            {
                mesh.childBecomesItsParent(face1BIdx);
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

            mesh.leftTUpdateInterval(rightTParentIdx, totalRelativeLeft);
            mesh.rightTUpdateInterval(newBottomEdgeIdx, totalRelativeRight - 1.0f, totalRelativeRight);
            mesh.setChildrenNewParent(*bottomRightEdge, newBottomEdgeIdx);

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

            mesh.getHandle(bottomRightEdge->handleIdxs.first) *= (1.0f / (1.0f - bottomEdgeT));
            mesh.getHandle(bottomRightEdge->handleIdxs.second) = mesh.handles[face1B.handleIdxs.second] * (1.0f / bottomEdgeT);
            bottomRightEdge->nextIdx = face1LIdx; // important !!!

            // mesh.copyEdge(face1BIdx, face2BIdx);
            face1B.createBar(-1, {0, 1});
            mesh.transferChildTo(face2BIdx, face1BIdx);
            // mesh.edges[newBottomEdgeIdx].replaceChild(face2BIdx, face1BIdx);

            mesh.leftTUpdateInterval(newBottomEdgeIdx, totalCurve);
            test2 = false;
            break;
        }
        case LeftT | RightL:
        {
            // 1 -> 24 -> 31 or 0 --> 47 --> 14 or 17 --> 37 --> 23
            mesh.transferChildToWithoutGeometry(face2RIdx, face1RIdx);
            mesh.transferChildToWithoutGeometry(face2BIdx, face1B.parentIdx);
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

            mesh.rightTUpdateInterval(newBottomEdgeIdx, newCurvePart, totalCurve);
            face1B.interval.x = 0;
            face1B.color = face2B.color;
            break;
        }
        case LeftL | RightL:
        case RightL:
        {
            // with LeftL: 2 --> 49 --> 16
            appState.bottomEdgeCase = "RightL (w/ or w/o LeftL)";
            mesh.transferChildToWithoutGeometry(face2RIdx, face1RIdx);
            mesh.transferChildToWithoutGeometry(face2BIdx, face1BIdx);
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
        mesh.setNextRightL(face2R, face1BIdx);

        appState.topEdgeT = addTopT ? mesh.addTJunction(*topRightEdge, *topLeftEdge, newTopEdgeIdx, 1.0f - topEdgeT) : 0;
        appState.bottomEdgeT = addBottomT ? mesh.addTJunction(*bottomLeftEdge, *bottomRightEdge, newBottomEdgeIdx, bottomEdgeT) : 0;

        mesh.copyEdgeTwin(face1RIdx, face2RIdx);
        mesh.removeFace(face2L.faceIdx);
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