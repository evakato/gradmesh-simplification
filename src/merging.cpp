#include "gradmesh.hpp"

namespace Merging
{
    bool merge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, int edgeId)
    {
        if (candidateMerges.size() > 0 && edgeId != -1)
        {
            int selectedHalfEdgeIdx = candidateMerges[edgeId].halfEdgeIdx1;
            mergePatches(mesh, selectedHalfEdgeIdx);
            candidateMerges = candidateEdges(mesh);
            return true;
        }
        return false;
    }

    void scaleRightTHandles(GradMesh &mesh, HalfEdge &edge, HalfEdge &other, float t)
    {
        auto &topLeftHandle = mesh.getHandle(edge.handleIdxs.first);
        auto &topRightHandle = mesh.getHandle(edge.handleIdxs.second);
        topLeftHandle = mesh.getHandle(other.handleIdxs.first) * (1.0f / t);
        topRightHandle *= (1.0f / (1.0f - t));
    }

    int chooseEdge(const std::vector<DoubleHalfEdge> &candidateEdgeList)
    {
        if (candidateEdgeList.size() > 0)
            return candidateEdgeList[0].halfEdgeIdx1;
        return -1;
    }

    std::vector<DoubleHalfEdge> candidateEdges(const GradMesh &mesh)
    {
        std::vector<DoubleHalfEdge> edgeList = {};
        int faceIdx = 0;
        for (auto &face : mesh.getFaces())
        {
            if (face.halfEdgeIdx != -1)
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
                            edgeList[foundIndex].curveId2 = CurveId{faceIdx, i};
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

    void scaleHandles(Handle &h1, Handle &h2, Handle &f2h, float t)
    {
        float leftMul = 1.0f / t;
        float rightMul = 1.0f / (1.0f - t);
        h1 *= leftMul;
        h2 = f2h * rightMul;
    }

    bool removeWholeTJunction(HalfEdge &bar1, HalfEdge &bar2)
    {
        bar1.interval.x = std::min(bar1.interval.x, bar2.interval.x);
        bar1.interval.y = std::max(bar1.interval.y, bar2.interval.y);
        // TODO : replace with approximations
        return (bar1.interval.x == 0 && bar1.interval.y == 1);
    }

    int getCornerFlags(bool topLeftL, bool topLeftT, bool topRightL, bool topRightT)
    {
        int flags = None;
        if (topLeftL)
            flags |= LeftL;
        if (topLeftT)
            flags |= LeftT;
        if (topRightL)
            flags |= RightL;
        if (topRightT)
            flags |= RightT;
        return flags;
    }

    void mergePatches(GradMesh &mesh, int mergeEdgeIdx)
    {
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
        int topLeftEdgeIdx = face1e4Idx;
        auto *topRightEdge = &face2e2;
        auto *bottomLeftEdge = &face1e2;
        int bottomLeftEdgeIdx = face1e2Idx;
        auto *bottomRightEdge = &face2e4;
        int bottomRightEdgeIdx = face2e4Idx;

        bool test = true;
        bool test2 = true;

        if (face1e1.isStem() || face2e1.isStem())
            return;

        const float t1 = splittingFactor(mesh, face1e1, face1e2, face2e2, 1);
        const float t2 = splittingFactor(mesh, face2e1, face1e2, face2e4, 1);
        float t = 0.5 * (t1 + t2);
        std::cout << "final t: " << t << std::endl;

        float topEdgeT = t;
        float bottomEdgeT = t;

        float modifiedScaleHandleRight = 1.0f / (1.0f - t);
        float modifiedScaleHandleLeft = 1.0f / t;

        float bottomTJunctionParameter = t;

        bool topLeftL = face1e3.isBar() && face1e4.isStem();
        bool topLeftT = face1e4.isBar() && mesh.twinIsStem(face1e3);
        bool topRightL = face2e3.isBar() && mesh.twinIsStem(face2e2);
        bool topRightT = face2e2.isBar() && face2e3.isStem();

        int topEdge = getCornerFlags(topLeftL, topLeftT, topRightL, topRightT);

        switch (topEdge)
        {
        case LeftL | RightL:
        {
            // 2 --> 49 --> 7 or 18 --> 35 --> 16
            // weird case lmao: basically just delete the stem from the RightL
            std::cout << "Case: LeftL && RightL" << std::endl;
            face1e1.copyChildData(face2e3);
            break;
        }
        case LeftT | RightT:
        {
            // 9 --> 18 --> 8
            std::cout << "Case: LeftT && RightT" << std::endl;
            topLeftEdgeIdx = face1e4.parentIdx;
            topLeftEdge = &mesh.edges[topLeftEdgeIdx];
            int rightTParentIdx = face2e2.parentIdx;
            topRightEdge = &mesh.edges[rightTParentIdx];

            auto [rightCurveBar1, rightCurveBar2, totalCurve] = parameterizeTPair((1.0f - t) / t, face1e4, face2e2);
            topEdgeT = 1.0f / totalCurve;

            // reparameterize the intervals for the right T children
            for (int childIdx : topRightEdge->childrenIdxs)
            {
                mesh.edges[childIdx].parentIdx = topLeftEdgeIdx;
                mesh.edges[childIdx].interval *= (rightCurveBar1 + rightCurveBar2);
                adjustInterval(mesh.edges[childIdx].interval, 1.0f, totalCurve);
            }

            // reparameterize the intervals for the left T children
            for (int childIdx : topLeftEdge->childrenIdxs)
            {
                mesh.edges[childIdx].interval /= totalCurve;
            }
            // left T inherits the children from right T
            mesh.edges[rightTParentIdx].replaceChild(face2e2Idx, face1e4Idx);
            mesh.edges[rightTParentIdx].replaceChild(face2e3Idx, face1e1Idx);
            topLeftEdge->addChildrenIdxs(topRightEdge->childrenIdxs);
            topLeftEdge->nextIdx = topRightEdge->nextIdx;

            // update new children and disable right T parent
            face1e1.copyChildData(face2e3);
            face1e4.interval.y = face2e2.interval.y;
            topRightEdge->disable();
            break;
        }
        case LeftT | RightL:
            // 9 --> 47 --> 8 or 16 -> 38 --> 26
            mesh.edges[face1e4.parentIdx].copyChildData(face2e2);
            face1e1.copyChildData(face2e3);
            [[fallthrough]];
        case LeftT:
        {
            std::cout << "Case: LeftT (w/ or w/o) RightL" << std::endl;
            topLeftEdgeIdx = face1e4.parentIdx;
            topLeftEdge = &mesh.edges[topLeftEdgeIdx];

            float newCurvePart = ((1.0f - t) / t) * (1.0f - face1e4.interval.x);
            topEdgeT = 1.0f / (1.0f + newCurvePart);

            for (int childIdx : topLeftEdge->childrenIdxs)
                mesh.edges[childIdx].interval /= (1.0f + newCurvePart);

            break;
        }
        case LeftL:
        {
            std::cout << "Case: LeftL && no RightL or RightT" << std::endl;
            // needs nothing
            break;
        }
        case RightL:
        {
            std::cout << "Case: RightL && no LeftL or LeftT" << std::endl;
            face1e1.copyChildData(face2e3);
            mesh.edges[face2e3.parentIdx].replaceChild(face2e3Idx, face1e1Idx);
            break;
        }
        case LeftL | RightT:
            // if top left edge is a stem, turn the parent into that stem
            // 2 --> 20 --> 7 (no twisting head) or 3 --> 24 --> 36
            mesh.edges[face2e2.parentIdx].copyChildData(face1e4);
            [[fallthrough]];
        case RightT:
        {
            // 41 --> 23
            // keep the parent, make the top left edge the bar1
            std::cout << "Case: Right T" << std::endl;
            int rightTParentIdx = face2e2.parentIdx;
            auto &rightTParent = mesh.edges[rightTParentIdx];

            float newCurvePart = (t / (1.0f - t)) * face2e2.interval.y;
            float totalCurve = 1.0f + newCurvePart;
            float newT = newCurvePart / totalCurve;

            scaleRightTHandles(mesh, rightTParent, face1e4, newT);

            rightTParent.copyGeometricDataExceptHandles(face1e4);
            face1e1.copyAll(face2e3);
            face1e4.copyParentAndHandles(face2e2);
            face1e4.originIdx = -1;

            mesh.edges[rightTParentIdx].replaceChild(face2e2Idx, face1e4Idx);
            mesh.edges[rightTParentIdx].replaceChild(face2e3Idx, face1e1Idx);

            for (int childIdx : rightTParent.childrenIdxs)
            {
                adjustInterval(mesh.edges[childIdx].interval, newCurvePart, totalCurve);
            }
            face1e4.interval.x = 0;

            mesh.addTJunction(rightTParent, face1e4, rightTParentIdx, 1.0f - newT);

            test = false;
            break;
        }
        case None:
        {
            std::cout << "Case: None" << std::endl;
            break;
        }
        default:
        {
            std::cout << "Unhandled case" << std::endl;
            break;
        }
        }

        bool bottomLeftL = face1e3.isBar() && mesh.twinIsStem(face1e2);
        bool bottomLeftT = face1e3.isStem() && face1e2.isBar();
        bool bottomRightL = face2e3.isBar() && face2e4.isStem();
        bool bottomRightT = face2e4.isBar() && mesh.twinIsStem(face2e3);
        int bottomEdge = getCornerFlags(bottomLeftL, bottomLeftT, bottomRightL, bottomRightT);
        switch (bottomEdge)
        {
        case LeftL | RightL:
        {
            std::cout << "Case: LeftL && RightL bottom" << std::endl;
            // 2 --> 49 --> 16
            face1e1.copyChildData(face2e3);
            face1e2.copyChildData(face2e4);
            break;
        }
        case LeftL | RightT:
        {
            // 3 -> 22 -> 51
            // 1 -> 31 -> 9 (now i don't have to twist my head)
            std::cout << "Case: LeftL && RightT bottom" << std::endl;
            // the bottom left edge is not a stem.. its twin is a stem
            // the bottom left edge inherits the bar data from the bottom right edge
            int rightTParentIdx = face2e4.parentIdx;
            auto &rightTParent = mesh.edges[rightTParentIdx];

            float newCurvePart = (1.0f - face2e4.interval.x) * (t / (1.0f - t));
            float totalCurve = 1.0f + newCurvePart;
            bottomEdgeT = newCurvePart / totalCurve;

            mesh.getHandle(rightTParent.handleIdxs.first) *= (1.0f / (1.0f - bottomEdgeT));
            mesh.getHandle(rightTParent.handleIdxs.second) = mesh.handles[face1e2.handleIdxs.second] * (1.0f / bottomEdgeT);
            rightTParent.nextIdx = face1e3Idx; // important !!!

            face1e2.copyAll(face2e4);

            mesh.edges[rightTParentIdx].replaceChild(face2e4Idx, face1e2Idx);
            for (int childIdx : rightTParent.childrenIdxs)
            {
                mesh.edges[childIdx].interval = mesh.edges[childIdx].interval / totalCurve;
            }
            face1e2.interval.y = 1.0f;
            mesh.addTJunction(face1e2, rightTParent, rightTParentIdx, bottomEdgeT);
            // bottomRightEdgeIdx = face2e4.parentIdx;
            test2 = false;
            break;
        }
        case LeftT | RightL:
        {
            // 1 -> 24 -> 31 or 0 --> 47 --> 14 or 17 --> 37 --> 23
            face1e1.copyChildData(face2e3);
            mesh.edges[face1e2.parentIdx].copyChildData(face2e4);
            [[fallthrough]];
        }
        case LeftT:
        {
            std::cout << "Case: LeftT && RightL bottom" << std::endl;
            float newCurvePart = (1.0f - t) / t * face1e2.interval.y;
            float totalCurve = 1.0f + newCurvePart;

            bottomLeftEdgeIdx = face1e2.parentIdx;
            bottomLeftEdge = &mesh.edges[bottomLeftEdgeIdx];

            face1e2.color = face2e4.color; // ok like why do bars even have color kms

            for (int childIdx : bottomLeftEdge->childrenIdxs)
            {
                adjustInterval(mesh.edges[childIdx].interval, newCurvePart, totalCurve);
            }
            face1e2.interval.x = 0;
            bottomEdgeT = 1.0f / totalCurve;
            break;
        }
        case LeftT | RightT:
        {
            // 0 -> 20 -> 14
            std::cout << "Case: LeftT && RightT" << std::endl;
            bottomLeftEdgeIdx = face1e2.parentIdx;
            bottomLeftEdge = &mesh.edges[bottomLeftEdgeIdx];
            int rightTParentIdx = face2e4.parentIdx;
            bottomRightEdge = &mesh.edges[rightTParentIdx];

            float rightCurveBar2 = (1.0f - t) / t * face1e2.interval.y;
            float s = face2e4.interval.x;
            float rightCurveBar1 = s / (1.0f - s) * rightCurveBar2;
            float totalCurve = 1.0f + rightCurveBar1 + rightCurveBar2;
            bottomEdgeT = 1.0f / totalCurve;

            auto [leftCurveBar1, leftCurveBar2, leftCurveTotal] = parameterizeTPair(t / (1.0f - t), face2e4, face1e2);

            // reparameterize the intervals for the right T children
            for (int childIdx : bottomRightEdge->childrenIdxs)
            {
                mesh.edges[childIdx].parentIdx = bottomLeftEdgeIdx;
                mesh.edges[childIdx].interval /= leftCurveTotal;
            }
            // reparameterize the intervals for the left T children
            for (int childIdx : bottomLeftEdge->childrenIdxs)
            {
                adjustInterval(mesh.edges[childIdx].interval, (rightCurveBar1 + rightCurveBar2), totalCurve);
            }
            mesh.edges[rightTParentIdx].replaceChild(face2e4Idx, face1e2Idx);
            bottomLeftEdge->addChildrenIdxs(bottomRightEdge->childrenIdxs);

            // update new children and disable right T parent
            face1e2.interval.x = face2e4.interval.x;
            face1e2.color = face2e4.color;
            bottomRightEdge->disable();
            break;
        }
        case LeftL:
        {
            std::cout << "Case: LeftL && no RightL or RightT" << std::endl;
            break;
        }
        case RightL:
        {
            std::cout << "Case: RightL && no LeftL or LeftT" << std::endl;
            face1e1.copyChildData(face2e3);
            face1e2.copyChildData(face2e4);
            mesh.edges[face2e3.parentIdx].replaceChild(face2e3Idx, face1e1Idx);
            mesh.edges[face2e4.parentIdx].replaceChild(face2e4Idx, face1e2Idx);
            break;
        }
        case RightT:
        {
            std::cout << "Case: bottom right T" << std::endl;
        }
        case None:
        {
            std::cout << "Case: None" << std::endl;
            break;
        }
        default:
        {
            std::cout << "Unhandled case" << std::endl;
            break;
        }
        }

        if (test)
        {
            float topLeftScale = 1.0f / topEdgeT;
            float topRightScale = 1.0f / (1.0f - topEdgeT);
            auto &topLeftHandle = mesh.getHandle(topLeftEdge->handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topLeftEdge->handleIdxs.second);
            topLeftHandle *= topLeftScale;
            topRightHandle = mesh.getHandle(topRightEdge->handleIdxs.second) * topRightScale;

            face1e1.copyGeometricData(face2e3);
            mesh.addTJunction(*topRightEdge, *topLeftEdge, topLeftEdgeIdx, 1.0f - topEdgeT);
        }

        if (test2)
        {
            float bottomLeftScale = 1.0f / bottomEdgeT;
            float bottomRightScale = 1.0f / (1.0f - bottomEdgeT);
            bottomLeftEdge->copyGeometricDataExceptHandles(*bottomRightEdge);
            auto &bottomLeftHandle = mesh.getHandle(bottomLeftEdge->handleIdxs.second);
            auto &bottomRightHandle = mesh.getHandle(bottomLeftEdge->handleIdxs.first);
            bottomLeftHandle *= bottomLeftScale;
            bottomRightHandle = mesh.getHandle(bottomRightEdge->handleIdxs.first) * bottomRightScale;
            mesh.addTJunction(*(bottomLeftEdge), *(bottomRightEdge), bottomLeftEdgeIdx, bottomEdgeT);
        }

        mesh.copyEdgeTwin(face1e1Idx, face2e3Idx);
        mesh.removeFace(face2e1.faceIdx);
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
        std::cout << "r1: " << r1 << " r2: " << r2 << " t: " << t << "\n";
        return t;
    }

}