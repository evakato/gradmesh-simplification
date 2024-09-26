#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "gradmesh.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMesh;

namespace Merging
{
    void merge(GradMesh &mesh);
    std::vector<int> candidateEdges(const GradMesh &mesh, const std::vector<Face> &faces);
    int chooseEdge(const std::vector<int> &candidateEdgeList);
    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign);
    void mergePatches(GradMesh &mesh, int halfEdgeIdx);

    void copyNormalHalfEdge(GradMesh &mesh, HalfEdge &e1, HalfEdge &e2);
    void scaleHandles(Handle &h1, Handle &h2, Handle &f2h, float t);
    void mergeStem(HalfEdge &bar1, HalfEdge &bar2);

    inline glm::vec2 absSum(glm::vec2 coords1, glm::vec2 coords2)
    {
        return {std::abs(coords1.x) + std::abs(coords2.x),
                std::abs(coords1.y) + std::abs(coords2.y)};
    }
    inline bool twoChildrenSameParent(int p1Idx, int p2Idx)
    {
        return (p1Idx != -1 && p2Idx != -1 && p1Idx == p2Idx);
    }
}