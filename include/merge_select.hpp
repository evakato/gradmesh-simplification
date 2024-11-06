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
    void findCandidateMerges();
    int selectEdge();

    bool validEdgeType(const HalfEdge &edge) const;
    void reset()
    {
        cornerEdges = {-1, -1};
        currAdjPair = {-1, -1};
        otherDirEdges.clear();
        otherDirIdx = 0;
        verticalDir = false;
        firstRow = true;
    }
    bool checkCycle(int edgeIdx) const;

private:
    int selectRandomEdge();
    int selectGridEdge();
    int selectDualGridEdge();

    GmsAppState &state;
    std::vector<int> selectedEdgePool; // for random selection

    std::pair<int, int> cornerEdges = {-1, -1}; // for grid selection
    std::pair<int, int> currAdjPair = {-1, -1};
    std::vector<int> otherDirEdges;
    bool verticalDir = false;
    int otherDirIdx = 0;

    bool firstRow = true;
};
