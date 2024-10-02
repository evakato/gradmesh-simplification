#pragma once

#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "merging.hpp"
#include "ostream_ops.hpp"
#include "patch.hpp"
#include "types.hpp"

Vertex interpolateCubic(CurveVector curve, float t);
Vertex interpolateCubicDerivative(CurveVector curve, float t);

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
    const std::vector<Face> &getFaces() const { return faces; }
    const Handle &getHandle(int idx)
    {
        assert(idx != -1 && "Handle idx is -1");
        return handles[idx];
    }
    const HalfEdge &getEdge(int idx) const
    {
        assert(idx != -1 && "Edge idx is -1");
        return edges[idx];
    }
    const Vertex getParentTangent(const HalfEdge &currEdge, int handleNum)
    {
        CurveVector parentCurve = getCurve(currEdge.parentIdx);
        float scale = currEdge.interval[1] - currEdge.interval[0];
        return interpolateCubicDerivative(parentCurve, currEdge.interval[handleNum]) * scale;
    }
    const Vertex getTangent(const HalfEdge &currEdge, int handleNum)
    {
        assert(handleNum == 0 || handleNum == 1);
        int handleIdx;
        if (handleNum == 0)
            handleIdx = currEdge.handleIdxs.first;
        else if (handleNum == 1)
            handleIdx = currEdge.handleIdxs.second;

        if (handleIdx != -1)
            return handles[handleIdx];

        std::cout << "have to get a parent and all that ";
        return getParentTangent(currEdge, handleNum);
    }
    void replaceChildWithParent(int childEdgeIdx);
    void addTJunction(int bar1Idx, int bar2Idx, int stemIdx, int parentTwinIdx, float t, bool extendStem);

    void disablePoint(int pointIdx)
    {
        points[pointIdx].halfEdgeIdx = -1;
    }
    bool halfEdgeIsParent(int halfEdgeIdx) const
    {
        if (halfEdgeIdx == -1)
            return false;
        return edges[halfEdgeIdx].childrenIdxs.size() > 0;
    }
    void rescaleChildren(const HalfEdge &e, float rescalingFactor)
    {
        for (int idx : e.childrenIdxs)
        {
            auto &childEdge = edges[idx];
            childEdge.interval *= rescalingFactor;
        }
    }
    void copyEdgeTwin(int e1Idx, int e2Idx)
    {
        auto &e1 = edges[e1Idx];
        auto &e2 = edges[e2Idx];
        e1.twinIdx = e2.twinIdx;
        if (e2.twinIdx != -1)
            edges[e2.twinIdx].twinIdx = e1Idx;
    }
    std::vector<Vertex> getHandles() const;

    void fixEdges();

    std::shared_ptr<std::vector<Patch>> generatePatchData();

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend bool Merging::merge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, int edgeId);
    friend void Merging::mergePatches(GradMesh &mesh, int halfEdgeIdx);

private:
    CurveVector getCurve(int halfEdgeIdx) const;
    std::array<Vertex, 4> computeEdgeDerivatives(const HalfEdge &edge) const;

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::shared_ptr<std::vector<Patch>> patches;
};
