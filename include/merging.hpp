#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gms_math.hpp"
#include "gradmesh.hpp"
#include "patch.hpp"
#include "types.hpp"
#include "window.hpp"

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

        int getHalfEdgeIdx() const
        {
            return halfEdgeIdx1;
        }
    };

    bool mergeAtSelectedEdge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, GmsAppState &appState);

    std::vector<DoubleHalfEdge> candidateEdges(const GradMesh &mesh);

    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign);
    void mergePatches(GradMesh &mesh, int halfEdgeIdx, GmsAppState &appState);

    enum CornerTJunctions
    {
        None = 0,
        LeftL = 1 << 0,  // 0001
        LeftT = 1 << 1,  // 0010
        RightL = 1 << 2, // 0100
        RightT = 1 << 3, // 1000
    };

    inline int getCornerTJunctions(bool topLeftL, bool topLeftT, bool topRightL, bool topRightT)
    {
        int flags = None;
        if (topLeftL)
            flags |= LeftL;
        if (topLeftT)
            flags |= LeftT;
        if (topRightL)
            flags |= RightL;
        if (topRightT)
            flags |= RightT;
        return flags;
    }

}