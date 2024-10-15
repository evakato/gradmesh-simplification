#pragma once

#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "gms_math.hpp"
#include "merging.hpp"
#include "ostream_ops.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMesh
{
public:
    GradMesh() = default;

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

    void fixEdges();
    std::shared_ptr<std::vector<Patch>> generatePatchData();
    std::vector<Vertex> getHandleBars() const;

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend class GradMeshMerger;

private:
    CurveVector getCurve(int halfEdgeIdx) const;
    std::array<Vertex, 4> computeEdgeDerivatives(const HalfEdge &edge) const;
    void disablePoint(int pointIdx)
    {
        if (pointIdx >= 0)
            points[pointIdx].halfEdgeIdx = -1;
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
        return edgeIs(e.twinIdx, &HalfEdge::isStem);
    }

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::shared_ptr<std::vector<Patch>> patches;
};
