#pragma once

#include <utility>
#include <vector>

#include "gms_appstate.hpp"

class GmsAppState;

// Facilitates the GradMeshMerger class in selecting a specific edge for a merge
class MergeSelect
{
public:
    MergeSelect(GmsAppState &state);
    void findCandidateMerges(std::vector<CurveId> *boundaryEdges = nullptr);
    int selectEdge();

    bool validEdgeType(const HalfEdge &edge) const;
    void reset();
    bool checkCycle(int edgeIdx) const;
    void preprocess();

private:
    int selectRandomEdge();
    int selectGridEdge();
    int selectDualGridEdge();
    int selectVerticalGridEdge();

    GmsAppState &state;
    std::vector<int> selectedEdgePool; // for random selection

    std::pair<int, int> cornerEdges = {-1, -1}; // for grid selection
    std::pair<int, int> currAdjPair = {-1, -1};
    std::vector<int> otherDirEdges;
    bool verticalDir = false;
    int otherDirIdx = 0;

    bool firstRow = true;

    std::vector<EdgeRegion> sortedRegions;
    int sortedRegionsIdx = 0;
    bool selectNewRegion = true;
    std::pair<int, int> maxRegion = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    std::pair<int, int> currGridIdxs = {0, 0};
};
