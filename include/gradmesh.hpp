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
using Region = std::vector<std::pair<int, int>>;

struct RegionAttributes
{
    std::pair<int, int> maxRegion = {0, 0};
    float error = 1.0f;
    int maxChainLength = 0;
    AABB maxRegionAABB;
    int getMaxPatches() const { return (maxRegion.first + 1) * (maxRegion.second + 1); }
};

struct TPRNode
{
    std::pair<int, int> gridPair;
    std::pair<int, int> maxRegion = {0, 0};
    float error = 1.0f;
    int maxChainLength = 0;

    int id;
    int degree = 0;

    int getMaxPatches() const { return (maxRegion.first + 1) * (maxRegion.second + 1); }
    std::partial_ordering operator<=>(const TPRNode &other) const
    {
        if (auto cmp = getMaxPatches() <=> other.getMaxPatches(); cmp != 0)
            return cmp;
        return other.error <=> error;
        // return (getMaxPatches() / error) <=> (other.getMaxPatches() / other.error);
        // return degree <=> other.degree;
    }
};

struct EdgeRegion
{
    std::pair<int, int> gridPair;
    int faceIdx;
    std::vector<RegionAttributes> allRegionAttributes;

    std::partial_ordering operator<=>(const EdgeRegion &other) const
    {
        if (allRegionAttributes.empty() || other.allRegionAttributes.empty())
            return std::partial_ordering::equivalent;

        if (auto cmp = allRegionAttributes[0].getMaxPatches() <=> other.allRegionAttributes[0].getMaxPatches(); cmp != 0)
            return cmp;

        return other.allRegionAttributes[0].error <=> allRegionAttributes[0].error;
    }
    std::vector<TPRNode> generateAllTPRs() const
    {
        std::vector<TPRNode> allTPRs;
        for (auto &attr : allRegionAttributes)
        {
            allTPRs.push_back(TPRNode{gridPair, attr.maxRegion, attr.error, attr.maxChainLength});
        }
        return allTPRs;
    }
    void setAndSortAttributes(std::vector<RegionAttributes> attributes)
    {
        allRegionAttributes = attributes;
        std::ranges::sort(allRegionAttributes, std::greater{}, &RegionAttributes::getMaxPatches);
    }
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
    bool isULMergeEdge(const HalfEdge &edge) const;

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
    AABB getProductRegionAABB(const std::pair<int, int> &gridPair, const std::pair<int, int> &maxRegion) const;

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
    std::vector<int> getIncidentFacesOfRegion(const Region &region) const;
    bool regionsOverlap(const Region &region1, const Region &region2) const;

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::vector<int> ulPointIdxs;
};