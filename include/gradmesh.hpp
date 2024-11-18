#pragma once

#include <cassert>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include "gms_math.hpp"
#include "ostream_ops.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMeshMerger;

using EdgeDerivatives = std::optional<std::array<Vertex, 4>>;

struct EdgeRegion
{
    std::pair<int, int> gridPair;
    std::pair<int, int> maxRegion = {0, 0};
    int faceIdx;
    AABB maxRegionAABB;
    int maxChainLength = 0;
    int getMaxPatches() const { return (maxRegion.first + 1) * (maxRegion.second + 1); }
};

class GradMesh
{
public:
    GradMesh() = default;
    ~GradMesh() = default;

    void addPoint(float x, float y, int idx)
    {
        points.push_back(Point{glm::vec2(x, y), idx});
    }
    void addHandle(float x, float y, float r, float g, float b, int idx)
    {
        handles.push_back(Handle{glm::vec2(x, y), glm::vec3(r, g, b), idx});
    }
    void addFace(int idx)
    {
        faces.push_back(Face{idx});
    }
    int addEdge(HalfEdge edge)
    {
        edges.push_back(edge);
        return edges.size() - 1;
    }
    const auto &getEdges() const
    {
        return edges;
    }
    const auto &getFaces() const
    {
        return faces;
    }
    const auto &getHandles() const
    {
        return handles;
    }
    const auto &getPoints() const
    {
        return points;
    }
    int getLeftMostChild(const HalfEdge &parent) const
    {
        for (int childIdx : parent.childrenIdxs)
            if (edges[childIdx].isLeftMostChild())
                return childIdx;
        return -1;
    }
    int getTwinIdx(int halfEdgeIdx) const
    {
        return edges[halfEdgeIdx].twinIdx;
    }
    int getNextRowIdx(int halfEdgeIdx) const
    {
        auto &e = edges[halfEdgeIdx];
        return edges[edges[e.nextIdx].twinIdx].nextIdx;
    }

    void fixEdges();
    std::optional<std::vector<Patch>> generatePatches() const;
    std::vector<Vertex> getHandleBars() const;
    std::vector<Vertex> getControlPoints() const;

    std::array<int, 4> getFaceEdgeIdxs(int edgeIdx) const;
    AABB getBoundingBoxOverFaces(std::vector<int> halfEdgeIdxs) const;
    AABB getAABB() const;
    std::vector<std::pair<int, int>> findCornerFace() const;
    std::vector<std::pair<int, int>> getGridEdgeIdxs(int rowIdx, int colIdx) const;
    bool validEdgeType(const HalfEdge &edge) const
    {
        return (edge.hasTwin() && !edge.isBar() && !twinIsParent(edge) && !edge.isParent() && !isULMergeEdge(edge));
    }
    int maxDependencyChain() const
    {
        int maxChain = 0;
        for (auto &edge : edges)
        {
            if (!edge.isValid() || !edge.isChild())
                continue;

            int newChain = 0;
            auto currEdge = edge;
            while (true)
            {
                if (currEdge.parentIdx == -1)
                    break;
                newChain++;
                currEdge = edges[currEdge.parentIdx];
            }
            maxChain = std::max(maxChain, newChain);
        }
        return maxChain;
    }

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend class GradMeshMerger;
    friend class MergeMetrics;
    friend class MergeSelect;
    friend class MergePreprocessor;

private:
    EdgeDerivatives getCurve(int halfEdgeIdx, int depth = 0) const;
    EdgeDerivatives computeEdgeDerivatives(const HalfEdge &edge, int depth = 0) const;
    void getMaxProductRegion(EdgeRegion &edgeRegion) const;

    void disablePoint(const HalfEdge &e)
    {
        if (e.originIdx != -1)
            points[e.originIdx].disable();
    }
    bool edgeIs(int edgeIdx, const auto &edgeFn) const
    {
        if (edgeIdx == -1)
            return false;
        return (edges[edgeIdx].*edgeFn)();
    }
    bool parentIsStem(const HalfEdge &e) const
    {
        return edgeIs(e.parentIdx, &HalfEdge::isStem);
    }
    bool twinIsParent(const HalfEdge &e) const
    {
        return edgeIs(e.twinIdx, &HalfEdge::isParent);
    }
    bool twinIsStem(const HalfEdge &e) const
    {
        int idx = e.twinIdx;
        if (e.isBar())
            idx = edges[e.twinIdx].parentIdx; // once again i'm bad
        return edgeIs(idx, &HalfEdge::isStem);
    }
    bool twinParentIsStem(const HalfEdge &e) const
    {
        int idx = e.twinIdx;
        if (e.isBar())
            idx = edges[e.parentIdx].twinIdx; // once again i'm bad

        const auto &twin = edges[idx];
        if (twin.isBar())
            return edgeIs(twin.parentIdx, &HalfEdge::isStem);
        else
            return edgeIs(idx, &HalfEdge::isStem);
    }
    AABB getFaceBoundingBox(int halfEdgeIdx) const;
    void findULPoints();
    bool isULMergeEdge(const HalfEdge &edge) const;
    int findParentPointIdx(int halfEdgeIdx) const;

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::vector<int> ulPointIdxs;
};