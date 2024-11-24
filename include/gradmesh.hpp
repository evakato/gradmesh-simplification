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
    float error = -1.0f;
    EdgeRegion(std::pair<int, int> gridPair,
               std::pair<int, int> maxRegion,
               int faceIdx,
               const AABB &aabb = AABB(),
               int maxChainLength = 0)
        : gridPair(gridPair),
          maxRegion(maxRegion),
          faceIdx(faceIdx),
          maxRegionAABB(aabb),
          maxChainLength(maxChainLength) {}
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
    const auto &getEdges() const { return edges; }
    const auto &getFaces() const { return faces; }
    const auto &getHandles() const { return handles; }
    const auto &getPoints() const { return points; }
    AABB getMeshAABB() const;

    std::optional<std::vector<Patch>> generatePatches() const;
    std::vector<Vertex> getHandleBars() const;
    std::vector<Vertex> getControlPoints() const;
    void fixEdges();

    // Returns the twin edge of the given edge
    int getTwinIdx(int halfEdgeIdx) const { return edges[halfEdgeIdx].twinIdx; }
    // Returns the index of the edge parallel and directly above the given edge, or -1 if none exists.
    int getNextRowIdx(int halfEdgeIdx) const;
    // Returns the 4 incident edges of a face given an edge on the face
    std::array<int, 4> getFaceEdgeIdxs(int edgeIdx) const;

    // Grid functions
    std::vector<std::pair<int, int>> findCornerFaces() const;
    std::vector<std::pair<int, int>> getGridEdgeIdxs(int rowIdx, int colIdx) const;

    bool validMergeEdge(const HalfEdge &edge) const
    {
        return (edge.isValid() && edge.hasTwin() && !edge.isBar() && !twinIsParent(edge) && !edge.isParent() && !isULMergeEdge(edge));
    }
    int maxDependencyChain() const;

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend class GradMeshMerger;
    friend class MergeMetrics;
    friend class MergeSelect;
    friend class MergePreprocessor;

private:
    EdgeDerivatives getCurve(int halfEdgeIdx, int depth = 0) const;
    EdgeDerivatives computeEdgeDerivatives(const HalfEdge &edge, int depth = 0) const;
    void getProductRegionAABB(EdgeRegion &edgeRegion) const;

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
    // AABB functions
    AABB getAffectedMergeAABB(int halfEdgeIdx) const;
    AABB getFaceAABB(int halfEdgeIdx) const;

    void findULPoints();
    bool isULMergeEdge(const HalfEdge &edge) const;

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::vector<int> ulPointIdxs;
};