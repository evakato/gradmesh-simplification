#pragma once

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "gradmesh.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMesh;

namespace Merging
{
    struct DoubleHalfEdge
    {
        int halfEdgeIdx1;
        int halfEdgeIdx2;
        CurveId curveId1;
        CurveId curveId2;
        DoubleHalfEdge(int heIdx1, int heIdx2, CurveId curveId1) : halfEdgeIdx1(heIdx1), halfEdgeIdx2(heIdx2), curveId1(curveId1) {};
    };

    bool merge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, int edgeId);
    std::vector<DoubleHalfEdge> candidateEdges(const GradMesh &mesh);

    int chooseEdge(const std::vector<DoubleHalfEdge> &candidateEdgeList);
    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign);
    void mergePatches(GradMesh &mesh, int halfEdgeIdx);

    void mergeStem(HalfEdge &bar1, HalfEdge &bar2);

    inline glm::vec2 absSum(glm::vec2 coords1, glm::vec2 coords2)
    {
        return {std::abs(coords1.x) + std::abs(coords2.x),
                std::abs(coords1.y) + std::abs(coords2.y)};
    }
    inline Vertex absVertexSum(Vertex v0, Vertex v1)
    {
        return Vertex{glm::abs(v0.coords) + glm::abs(v1.coords), glm::abs(v0.color) + glm::abs(v1.color)};
    }
    inline bool twoChildrenSameParent(int p1Idx, int p2Idx)
    {
        return (p1Idx != -1 && p2Idx != -1 && p1Idx == p2Idx);
    }

    // reparameterization functions
    inline auto parameterizeTBar2(float tRatio, const HalfEdge &relativeEdge)
    {
        float otherBarParam = tRatio * relativeEdge.interval.y;
        float totalParam = 1.0f + otherBarParam;
        return std::make_tuple(otherBarParam, totalParam);
    }
    inline float totalCurveRelativeRight(float tRatio, const HalfEdge &relativeEdge, const HalfEdge &paramEdge)
    {
        auto [bar1Param, unused] = parameterizeTBar2(tRatio, relativeEdge);
        float bar2Param = (paramEdge.interval.x / (1.0f - paramEdge.interval.x)) * bar1Param;
        return bar1Param + bar2Param + 1.0f;
    }
    inline auto parameterizeTBar1(float tRatio, const HalfEdge &relativeEdge)
    {
        float otherBarParam = tRatio * (1.0f - relativeEdge.interval.x);
        float totalParam = 1.0f + otherBarParam;
        return std::make_tuple(otherBarParam, totalParam);
    }
    inline float totalCurveRelativeLeft(float tRatio, const HalfEdge &relativeEdge, const HalfEdge &paramEdge)
    {
        auto [bar1Param, unused] = parameterizeTBar1(tRatio, relativeEdge);
        float bar2Param = (1.0f - paramEdge.interval.y) / paramEdge.interval.y * bar1Param;
        return bar1Param + bar2Param + 1.0f;
    }

    enum CornerFlags
    {
        None = 0,
        LeftL = 1 << 0,  // 0001
        LeftT = 1 << 1,  // 0010
        RightL = 1 << 2, // 0100
        RightT = 1 << 3  // 1000
    };

    inline int getCornerFlags(bool topLeftL, bool topLeftT, bool topRightL, bool topRightT)
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

}