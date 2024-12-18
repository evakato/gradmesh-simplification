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
    void findCandidateMerges(std::vector<SingleHalfEdge> *boundaryEdges = nullptr);
    int selectEdge();
    void reset();

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
};
