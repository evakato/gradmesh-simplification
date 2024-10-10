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
    std::vector<Vertex> getHandleBars() const;
    const std::vector<Face> &getFaces() const { return faces; }
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

        return getParentTangent(currEdge, handleNum);
    }
    void addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t);

    void disablePoint(int pointIdx)
    {
        points[pointIdx].halfEdgeIdx = -1;
    }
    bool twinIsTJunction(const HalfEdge &e) const
    {
        if (e.twinIdx == -1)
            return false;
        return edges[e.twinIdx].isParent();
    }
    void rescaleChildren(const HalfEdge &e, float rescalingFactor)
    {
        for (int idx : e.childrenIdxs)
        {
            auto &childEdge = edges[idx];
            childEdge.interval *= rescalingFactor;
        }
    }
    bool twinIsStem(const HalfEdge &e) const
    {
        if (e.twinIdx == -1)
            return false;
        return edges[e.twinIdx].isStem();
    }

    // Methods for merging faces
    void removeFace(int faceIdx);
    void copyEdgeTwin(int e1Idx, int e2Idx);
    Handle &getHandle(int idx)
    {
        assert(idx != -1 && "Handle idx is -1");
        return handles[idx];
    }
    void copyChild(int newChildIdx, int oldChildIdx)
    {
        auto &oldChild = edges[oldChildIdx];
        edges[newChildIdx].copyChildData(oldChild);
        if (oldChild.parentIdx != -1)
        {
            edges[oldChild.parentIdx].addChildrenIdxs({newChildIdx});
        }
    }
    void copyAndReplaceChild(int newChildIdx, int oldChildIdx)
    {
        auto &oldChild = edges[oldChildIdx];
        edges[newChildIdx].copyChildData(oldChild);
        edges[newChildIdx].copyGeometricData(oldChild);
        if (oldChild.parentIdx != -1)
        {
            edges[oldChild.parentIdx].replaceChild(oldChildIdx, newChildIdx);
        }
    }
    void copyEdge(int newChildIdx, int oldChildIdx)
    {
        edges[newChildIdx].copyGeometricData(edges[oldChildIdx]);
        copyChild(newChildIdx, oldChildIdx);
    }

    void fixEdges();

    void fixAndSetTwin(int barIdx);

    bool twinFaceIsCycle(const HalfEdge &e) const;

    std::shared_ptr<std::vector<Patch>> generatePatchData();

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);
    friend void Merging::mergePatches(GradMesh &mesh, int halfEdgeIdx, GmsAppState &appState);

private:
    CurveVector getCurve(int halfEdgeIdx) const;
    std::array<Vertex, 4> computeEdgeDerivatives(const HalfEdge &edge) const;
    void leftTUpdateInterval(int parentIdx, float totalCurve);
    void scaleDownChildrenByT(HalfEdge &parentEdge, float t);
    void scaleUpChildrenByT(HalfEdge &parentEdge, float t);
    void setBarChildrensTwin(HalfEdge &parentEdge, int twinIdx);
    void setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx);

    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;

    std::shared_ptr<std::vector<Patch>> patches;
};
