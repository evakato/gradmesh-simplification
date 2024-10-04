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

    void handleTJunction(GradMesh &mesh, const HalfEdge &edge1, const HalfEdge &edge2, int twinOfParentIdx, float t)
    {
        std::cout << "the twins are: " << edge1.twinIdx << "< " << edge2.twinIdx << std::endl;
        if (edge1.hasTwin() && edge2.hasTwin())
        {
            std::cout << "adding a t-junction\n";
            mesh.addTJunction(edge1.twinIdx, edge2.twinIdx, twinOfParentIdx, t, mesh.twinIsStem(edge1));
        }
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
        auto *bottomLeftEdge = &face1e2;
        int bottomLeftEdgeIdx = face1e2Idx;
        auto *bottomRightEdge = &face2e4;
        int bottomRightEdgeIdx = face2e4Idx;

        bool test = true;

        if (face1e1.isStem() || face2e1.isStem())
            return;

        const float t1 = splittingFactor(mesh, face1e1, face1e2, face2e2, 1);
        const float t2 = splittingFactor(mesh, face2e1, face1e2, face2e4, 1);
        const float t = 0.5 * (t1 + t2);
        std::cout << "final t: " << t << std::endl;
        float scaleHandleLeft = 1.0f / t;
        float scaleHandleRight = 1.0f / (1.0f - t);
        float modifiedScaleHandleRight = scaleHandleRight;
        float modifiedScaleHandleLeft = scaleHandleLeft;

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
            // 2 --> 49 --> 7
            // 18 --> 35 --> 16
            // weird case lmao
            // basically just delete the stem from the RightL
            std::cout << "Case: LeftL && RightL" << std::endl;
            // auto &topLeftHandle = mesh.getHandle(face1e4.handleIdxs.first);
            // auto &topRightHandle = mesh.getHandle(face1e4.handleIdxs.second);
            // topLeftHandle *= scaleHandleLeft;
            // topRightHandle = mesh.getHandle(face2e2.handleIdxs.second) * scaleHandleRight;

            face1e1.copyAll(face2e3);
            // face1e2.copyGeometricDataExceptHandles(face2e4);
            // handleTJunction(mesh, face2e2, face1e4, face1e4Idx, 1.0f - t);

            // test = false;
            break;
        }
        case LeftL | RightT:
        {
            // 3 --> 24 --> 36
            // 2 --> 20 --> 7 (no twisting head)
            // strategy: make the left stem the bar1, make the parent the stem
            std::cout << "Case: LeftL && RightT" << std::endl;
            auto &topRightTParentIdx = face2e2.parentIdx;
            auto &topRightTParent = mesh.edges[topRightTParentIdx];
            auto &leftStem = face1e4;

            float newCurvePart = (t / (1.0f - t)) * face2e2.interval.y;
            float reparameterize = 1.0f + newCurvePart;
            float totalCurve = (1.0f - face2e2.interval.y) / face2e2.interval.y * (1.0f - t) + 1.0f;
            float newT = t / totalCurve;
            std::cout << reparameterize << "\n";
            std::cout << totalCurve << "\n";
            std::cout << newT << "\n";

            auto &topLeftHandle = mesh.getHandle(topRightTParent.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topRightTParent.handleIdxs.second);
            topLeftHandle = mesh.getHandle(face1e4.handleIdxs.first) * (1.0f / newT);
            topRightHandle *= (1.0f / (1.0f - newT));

            topRightTParent.copyChildData(leftStem);
            topRightTParent.originIdx = -1;
            leftStem.copyParentAndHandles(face2e2);

            leftStem.interval.y = 1.0f - ((1.0f - face2e2.interval.y) / reparameterize);
            auto &stem = mesh.edges[face2e2.nextIdx];
            stem.interval = 1.0f - ((1.0f - stem.interval) / reparameterize);
            auto &otherBar = mesh.edges[mesh.edges[stem.twinIdx].nextIdx];
            otherBar.interval.x = 1.0f - ((1.0f - otherBar.interval.x) / reparameterize);
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);
            face1e1.copyGeometricData(face2e3);
            face1e1.copyChildData(face2e3);

            handleTJunction(mesh, topRightTParent, face1e4, topRightTParentIdx, 1.0f - newT);

            test = false;
            break;
        }
        case LeftT | RightL:
        {
            // 9 --> 47 --> 8
            // 16 -> 38 --> 26
            std::cout << "Case: LeftT && RightL" << std::endl;
            float newCurvePart = ((1.0f - t) / t) * (1.0f - face1e4.interval.x);
            float reparameterize = 1.0f + newCurvePart;
            float newT = 1.0f / reparameterize;
            int topLeftTParentIdx = face1e4.parentIdx;
            auto &topLeftTParent = mesh.edges[face1e4.parentIdx];
            auto &topLeftHandle = mesh.getHandle(topLeftTParent.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topLeftTParent.handleIdxs.second);
            topLeftHandle *= (1.0f / newT);
            topRightHandle = mesh.getHandle(face2e2.handleIdxs.second) * (1.0f / (1.0f - newT));

            for (int childIdx : topLeftTParent.childrenIdxs)
            {
                mesh.edges[childIdx].interval /= reparameterize;
            }
            topLeftTParent.copyChildData(face2e2);
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);
            face1e1.copyGeometricData(face2e3);
            face1e1.copyChildData(face2e3);
            handleTJunction(mesh, face2e2, topLeftTParent, topLeftTParentIdx, 1.0f - newT);

            test = false;
            break;
        }
        case LeftT | RightT:
        {
            // 9 --> 18 --> 8
            std::cout << "Case: LeftT && RightT" << std::endl;
            int topLeftTParentIdx = face1e4.parentIdx;
            auto &topLeftTParent = mesh.edges[topLeftTParentIdx];
            auto &topRightTParent = mesh.edges[face2e2.parentIdx];

            float rightLeftBarLength = (1.0f - t) / t * (1.0f - face1e4.interval.x);
            float s = face2e2.interval.y;
            float rightRightBarLength = (1.0f - s) / s * rightLeftBarLength;
            float totalCurve = 1.0f + rightLeftBarLength + rightRightBarLength;
            float d = (1.0f + rightLeftBarLength) / totalCurve;
            float newT = 1.0f / totalCurve;

            auto &topLeftHandle = mesh.getHandle(topLeftTParent.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topLeftTParent.handleIdxs.second);
            topLeftHandle *= (1.0f / newT);
            topRightHandle = mesh.getHandle(topRightTParent.handleIdxs.second) * (1.0f / (1.0f - newT));

            topLeftTParent.nextIdx = topRightTParent.nextIdx;
            for (int childIdx : topRightTParent.childrenIdxs)
            {
                mesh.edges[childIdx].parentIdx = topLeftTParentIdx;
            }
            for (int childIdx : topLeftTParent.childrenIdxs)
            {
                mesh.edges[childIdx].interval /= totalCurve;
            }
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);
            face1e1.copyGeometricData(face2e3);
            face1e1.interval = {d, d};
            face1e1.originIdx = -1;
            face1e1.parentIdx = topLeftTParentIdx;
            auto &otherChild = mesh.edges[mesh.edges[face2e3.twinIdx].nextIdx];
            otherChild.interval = {(1.0f + rightLeftBarLength) / totalCurve, 1.0f};

            handleTJunction(mesh, face2e2, topLeftTParent, topLeftTParentIdx, 1.0f - newT);
            topRightTParent.faceIdx = -1;
            test = false;
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
        case LeftT:
        {
            std::cout << "Case: Left T" << std::endl;
            int topLeftTParentIdx = face1e4.parentIdx;
            auto &topLeftTParent = mesh.edges[topLeftTParentIdx];
            auto &topLeftHandle = mesh.getHandle(topLeftTParent.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topLeftTParent.handleIdxs.second);
            float newCurvePart = ((1.0f - t) / t) * (1.0f - face1e4.interval.x);
            float newT = 1.0f / (1.0f + newCurvePart);
            topLeftHandle *= (1.0f / newT);
            topRightHandle = mesh.getHandle(face2e2.handleIdxs.second) * (1.0f / (1.0f - newT));
            face1e1.copyGeometricData(face2e3);
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);
            for (int childIdx : topLeftTParent.childrenIdxs)
            {
                mesh.edges[childIdx].interval /= (1.0f + newCurvePart);
            }
            handleTJunction(mesh, face2e2, topLeftTParent, topLeftTParentIdx, 1 - newT);
            test = false;
            break;
        }
        case RightT:
        {
            // 41 --> 23
            std::cout << "Case: Right T" << std::endl;
            // keep the parent, make the top left edge the bar1
            int topRightTParentIdx = face2e2.parentIdx;
            auto &topRightTParent = mesh.edges[topRightTParentIdx];
            float newCurvePart = (t / (1.0f - t)) * face2e2.interval.y;
            float totalCurve = 1.0f + newCurvePart;
            float adjustChild = (face2e2.interval.y + newCurvePart) / totalCurve;
            float newT = newCurvePart / totalCurve;

            auto &topLeftHandle = mesh.getHandle(topRightTParent.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(topRightTParent.handleIdxs.second);
            topLeftHandle = mesh.getHandle(face1e4.handleIdxs.first) * (1.0f / newT);
            topRightHandle *= (1.0f / (1.0f - newT));

            topRightTParent.copyGeometricDataExceptHandles(face1e4);
            face1e1.copyAll(face2e3);
            face1e4.copyParentAndHandles(face2e2);
            face1e4.originIdx = -1;
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);
            // once again this is scuffed but works for now..
            face1e4.interval = {0, adjustChild};
            face1e1.interval = {adjustChild, adjustChild};
            auto &otherBar = mesh.edges[mesh.edges[face2e3.twinIdx].nextIdx];
            otherBar.interval = {adjustChild, 1};
            handleTJunction(mesh, topRightTParent, face1e4, topRightTParentIdx, 1.0f / totalCurve);
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
        int flags2 = getCornerFlags(bottomLeftL, bottomLeftT, bottomRightL, bottomRightT);
        switch (flags2)
        {
        case LeftL | RightL:
        {
            std::cout << "Case: LeftL && RightL bottom" << std::endl;
            break;
        }
        case LeftL | RightT:
        {
            // 3 -> 22 -> 51
            // 1 -> 31 -> 9 (now i don't have to twist my head)
            std::cout << "Case: LeftL && RightT bottom" << std::endl;
            // the bottom left edge is not a stem.. its twin is a stem
            // the bottom left edge inherits the bar data from the bottom right edge
            face1e1.copyGeometricData(face2e3);
            int rightTParentIdx = face2e4.parentIdx;
            auto &rightTParent = mesh.edges[rightTParentIdx];

            float newCurvePart = (1.0f - face2e4.interval.x) * (t / (1.0f - t));
            float reparameterize = 1.0f + newCurvePart;
            float newT = newCurvePart / (1.0 + newCurvePart);
            // think of this as 1 / reparameterize is the length of the parent curve / the entire new curve
            // then do 1 / that for the scaling factor
            std::cout << newT << "\n";
            mesh.getHandle(rightTParent.handleIdxs.first) *= (1.0f / (1.0f - newT));
            mesh.getHandle(rightTParent.handleIdxs.second) = mesh.handles[face1e2.handleIdxs.second] * (1.0f / newT);
            // important !!!
            rightTParent.nextIdx = face1e3Idx;

            face1e2.copyAll(face2e4);
            face1e2.interval = glm::vec2(face2e4.interval.x / reparameterize, 1.0f);
            for (int childIdx : rightTParent.childrenIdxs)
            {
                mesh.edges[childIdx].interval = mesh.edges[childIdx].interval / reparameterize;
            }
            bottomRightEdge = &rightTParent;
            bottomRightEdgeIdx = face2e4.parentIdx;
            handleTJunction(mesh, *(bottomLeftEdge), *(bottomRightEdge), rightTParentIdx, newT);
            test = false;
            break;
        }
        case LeftT | RightL:
        {
            // 1 -> 24 -> 31
            std::cout << "Case: LeftT && RightL bottom" << std::endl;
            auto &parent = mesh.edges[face1e2.parentIdx];
            bottomLeftEdge = &parent;
            bottomLeftEdgeIdx = face1e2.parentIdx;
            parent.copyChildData(face2e4);
            face1e2.color = face2e4.color; // ok like why do bars even have color kms
            face1e1.copyChildData(face2e3);
            // face1e3 is the stem of LeftT, its interval needs to be updated
            float totalCurve = ((1.0f - face1e2.interval.y) / face1e2.interval.y) * t + 1.0f;
            float newCurvePart = (t / (1.0f - t)) * face1e2.interval.y;
            float reparameterize = 1.0f + newCurvePart;
            float newT = t / totalCurve;
            face1e3.interval = 1.0f - ((1.0f - face1e3.interval) / reparameterize);
            face1e2.interval.y = 1.0f - ((1.0f - face1e2.interval.y) / reparameterize);
            auto &otherBar = mesh.edges[mesh.edges[face1e3.twinIdx].nextIdx];
            otherBar.interval.x = 1.0f - ((1.0f - otherBar.interval.x) / reparameterize);
            modifiedScaleHandleRight = 1.0f / newT;
            modifiedScaleHandleLeft = 1.0f / (1.0f - newT);
            bottomTJunctionParameter = 1.0f - newT;
            break;
        }
        case LeftT | RightT:
        {
            // 0 -> 20 ->
            std::cout << "Case: LeftT && RightT" << std::endl;
            break;
        }
        case LeftL:
        {
            std::cout << "Case: LeftL && no RightL or RightT" << std::endl;
            std::cout << "bottom left sideways T" << std::endl;
            break;
        }
        case RightL:
        {
            std::cout << "Case: RightL && no LeftL or LeftT" << std::endl;
            std::cout << "bottom right sideways T" << std::endl;
            face1e1.copyChildData(face2e3);
            face1e2.copyChildData(face2e4);
            mesh.edges[face2e3.parentIdx].replaceChild(face2e3Idx, face1e1Idx);
            mesh.edges[face2e4.parentIdx].replaceChild(face2e4Idx, face1e2Idx);
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

        if (test)
        {
            auto &topLeftHandle = mesh.getHandle(face1e4.handleIdxs.first);
            auto &topRightHandle = mesh.getHandle(face1e4.handleIdxs.second);
            topLeftHandle *= scaleHandleLeft;
            topRightHandle = mesh.getHandle(face2e2.handleIdxs.second) * scaleHandleRight;

            face1e1.copyGeometricData(face2e3);
            bottomLeftEdge->copyGeometricDataExceptHandles(face2e4);

            std::cout << "merge edge: " << face1e1Idx << "\n";
            // i had this out of the test if block before but yeah
            handleTJunction(mesh, face2e2, face1e4, face1e4Idx, 1 - t);
        }

        auto &bottomLeftHandle = mesh.getHandle(bottomLeftEdge->handleIdxs.second);
        auto &bottomRightHandle = mesh.getHandle(bottomLeftEdge->handleIdxs.first);
        bottomLeftHandle *= modifiedScaleHandleLeft;
        bottomRightHandle = mesh.getHandle(bottomRightEdge->handleIdxs.first) * modifiedScaleHandleRight;
        handleTJunction(mesh, *(bottomLeftEdge), *(bottomRightEdge), bottomLeftEdgeIdx, bottomTJunctionParameter);

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