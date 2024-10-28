#pragma once

#include <vector>

#include "gms_appstate.hpp"

class GmsAppState;

class MergeSelect
{
public:
    MergeSelect(GmsAppState &state);
    void setDoubleHalfEdge() const;
    void findCandidateMerges();

    int selectEdge();
    void selectRandomEdge();
    int selectGridEdge();

    std::vector<DoubleHalfEdge> &getCandidateMerges()
    {
        return candidateMerges;
    };

private:
    GmsAppState &state;
    std::vector<int> selectedEdgePool;
    int currEdgeIdx;
    std::vector<DoubleHalfEdge> candidateMerges;

    std::vector<int> prevSeenEdges;
};