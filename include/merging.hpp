#pragma once

#include <algorithm>
#include <vector>

#include "gradmesh.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMesh;

namespace Merging
{
    void merge(GradMesh &mesh);
    int chooseEdge(const std::vector<Face> &faces, const std::vector<HalfEdge> &edges);
    const float splittingFactor(GradMesh &mesh, HalfEdge &face1e1, HalfEdge &face1e2, HalfEdge &face1e4, HalfEdge &face2e1, HalfEdge &face2e2, HalfEdge &face2e4);
    void mergePatches(GradMesh &mesh, int halfEdgeIdx);

    inline glm::vec2 absSum(glm::vec2 coords1, glm::vec2 coords2)
    {
        return {std::abs(coords1.x) + std::abs(coords2.x),
                std::abs(coords1.y) + std::abs(coords2.y)};
    }
}