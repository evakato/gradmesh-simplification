#pragma once

#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "merging.hpp"
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
    const Handle &getHandle(int idx)
    {
        assert(idx != -1 && "Handle idx is -1");
        return handles[idx];
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

        return getParentTangent(currEdge, handleNum);
    }
    void fixEdges();
    void candidateMerges();

    std::shared_ptr<std::vector<Patch>> generatePatchData();

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend void Merging::merge(GradMesh &mesh);
    friend void Merging::mergePatches(GradMesh &mesh, int halfEdgeIdx);

private:
    CurveVector getCurve(int halfEdgeIdx);
    std::array<Vertex, 4> computeEdgeDerivatives(const HalfEdge &edge);

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::shared_ptr<std::vector<Patch>> patches;
};
