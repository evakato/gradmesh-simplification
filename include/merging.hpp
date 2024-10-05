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

    void scaleHandles(Handle &h1, Handle &h2, Handle &f2h, float t);
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
    // used for finding the top right or bottom left T junction parameterization relative to the merge pair
    inline auto parameterizeTPair(float tRatio, const HalfEdge &relativeEdge, const HalfEdge &paramEdge)
    {
        float bar1Param = tRatio * (1.0f - relativeEdge.interval.x);
        float bar2Param = (1.0f - paramEdge.interval.y) / paramEdge.interval.y * bar1Param;
        float totalParam = 1.0f + bar1Param + bar2Param;
        return std::make_tuple(bar1Param, bar2Param, totalParam);
    }
    void scaleRightTHandles(GradMesh &mesh, HalfEdge &edge, HalfEdge &other, float t);

    template <typename T>
    void adjustInterval(T &interval, float newCurvePart, float totalCurve)
    {
        interval += newCurvePart;
        interval /= totalCurve;
    }

    enum CornerFlags
    {
        None = 0,
        LeftL = 1 << 0,  // 0001
        LeftT = 1 << 1,  // 0010
        RightL = 1 << 2, // 0100
        RightT = 1 << 3  // 1000
    };

}