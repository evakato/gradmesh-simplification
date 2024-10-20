#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gms_math.hpp"
#include "merge_metrics.hpp"
#include "patch.hpp"
#include "types.hpp"
#include "window.hpp"

class GradMesh;

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

enum MergeState
{
    FAILURE,
    SUCCESS,
    METRIC_ERROR
};

class GradMeshMerger
{
public:
    GradMeshMerger(GmsAppState &appState, int patchShaderId);
    MergeState mergeAtSelectedEdge();
    void findCandidateMerges();
    const DoubleHalfEdge &getDoubleHalfEdge(int idx) { return candidateMerges[idx]; }

private:
    GmsAppState::MergeStats mergePatches(int halfEdgeIdx);
    float splittingFactor(HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign) const;
    float addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t);

    void leftTUpdateInterval(int parentIdx, float totalCurve);
    void rightTUpdateInterval(int parentIdx, float reparam1, float reparam2);
    void scaleDownChildrenByT(HalfEdge &parentEdge, float t);
    void scaleUpChildrenByT(HalfEdge &parentEdge, float t);

    void setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx);
    void setParentChildrenTwin(HalfEdge &parentEdge, int newTwinIdx);
    void childBecomesItsParent(int childIdx);
    void setBarChildrensTwin(HalfEdge &parentEdge, int twinIdx);
    void setNextRightL(const HalfEdge &bar, int nextIdx);

    void transferChildTo(int oldChildIdx, int newChildIdx);
    void transferChildToWithoutGeometry(int oldChildIdx, int newChildIdx);
    void fixAndSetTwin(int barIdx);

    bool twinFaceIsRightCycle(const HalfEdge &e) const;
    bool twinFaceIsCycle(const HalfEdge &e, const HalfEdge &adjEdge) const;
    bool twinFaceIsCCWCycle(const HalfEdge &e) const;

    void copyEdgeTwin(int e1Idx, int e2Idx);
    void removeFace(int faceIdx);

    GradMesh &mesh;
    GmsAppState &appState;
    std::vector<DoubleHalfEdge> candidateMerges;
    MergeMetrics metrics;
};

enum CornerTJunctions
{
    None = 0,
    RightT = 1 << 0, // 0001
    LeftT = 1 << 1,  // 0010
    RightL = 1 << 2, // 0100
    LeftL = 1 << 3,  // 1000
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
};