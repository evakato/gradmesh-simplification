#include "merge_select.hpp"

MergeSelect::MergeSelect(GmsAppState &state) : state(state) {}

void MergeSelect::setDoubleHalfEdge() const
{
    int selectedIdx = state.selectedEdgeId;
    int prevIdx = state.prevSelectedEdgeId;
    if (selectedIdx >= 0 && selectedIdx < candidateMerges.size())
        state.selectedDhe = candidateMerges[selectedIdx];
    if (prevIdx >= 0 && prevIdx < candidateMerges.size())
        state.prevSelectedDhe = candidateMerges[prevIdx];
}

void MergeSelect::findCandidateMerges()
{
    candidateMerges.clear();
    int faceIdx = 0;
    auto mesh = state.mesh;
    for (auto &face : mesh.getFaces())
    {
        if (face.isValid())
        {
            int currIdx = face.halfEdgeIdx;
            for (int i = 0; i < 4; i++)
            {
                const auto &currEdge = mesh.edges[currIdx];
                if (currEdge.hasTwin() && !currEdge.isBar() && !mesh.twinIsParent(currEdge) && !currEdge.isParent())
                {
                    auto findEdgeIdx = std::find_if(candidateMerges.begin(), candidateMerges.end(), [currIdx](const DoubleHalfEdge &dhe)
                                                    { return dhe.halfEdgeIdx2 == currIdx; });

                    if (findEdgeIdx == candidateMerges.end())
                    {
                        candidateMerges.push_back(DoubleHalfEdge{currIdx, currEdge.twinIdx, CurveId{faceIdx, i}});
                    }
                    else
                    {
                        size_t foundIndex = std::distance(candidateMerges.begin(), findEdgeIdx);
                        // assert(currIdx != candidateMerges[foundIndex].halfEdgeIdx2)
                        candidateMerges[foundIndex]
                            .curveId2 = CurveId{faceIdx, i};
                    }
                }
                currIdx = currEdge.nextIdx;
            }
            faceIdx++;
        }
    }

    for (auto &doublehe : candidateMerges)
    {
        // std::cout << doublehe.halfEdgeIdx1 << ", " << doublehe.halfEdgeIdx2 << "\n";
    }
    state.numOfCandidateMerges = candidateMerges.size();
}

int MergeSelect::selectEdge()
{
    switch (state.mergeMode)
    {
    case MANUAL:
        return candidateMerges[state.selectedEdgeId].getHalfEdgeIdx();
    case RANDOM:
    {
        selectRandomEdge();
        return candidateMerges[state.selectedEdgeId].getHalfEdgeIdx();
    }
    case GRID:
    {
        return selectGridEdge();
    }
    }
    return -1;
}

void MergeSelect::selectRandomEdge()
{
    switch (state.mergeStatus)
    {
    case NA:
    case SUCCESS:
    {
        selectedEdgePool = generateRandomNums(state.numOfCandidateMerges - 1);
        state.attemptedMergesIdx = 0;
        break;
    }
    case CYCLE:
    case METRIC_ERROR:
    {
        if (state.attemptedMergesIdx >= selectedEdgePool.size())
        {
            state.mergeMode = NONE;
            return;
        }
        break;
    }
    }
    state.selectedEdgeId = selectedEdgePool[state.attemptedMergesIdx++];
}

int MergeSelect::selectGridEdge()
{
    auto [adj1, adj2] = state.mesh.findCornerFace();
    // while (true)
    // {
    int adj2Twin = adj2 == -1 ? -1 : state.mesh.getTwinIdx(adj2);
    int nextEdgeIdx = adj2Twin == -1 ? -1 : state.mesh.edges[adj2Twin].nextIdx;
    while (state.mesh.getTwinIdx(adj1) != -1)
    {
        const auto &currEdge = state.mesh.edges[adj1];
        if (!currEdge.hasTwin() || currEdge.isBar() || state.mesh.twinIsParent(currEdge) || currEdge.isParent())
        {
            adj1 = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(adj1))[2];
            continue;
        }

        return adj1;
        // state.mergeStatus = mergeAtSelectedEdge(adj1);
        if (state.mergeStatus != SUCCESS)
            adj1 = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(adj1))[2];
    }

    /*
            if (adj2Twin == -1)
                break;
            adj1 = nextEdgeIdx;
            adj2 = state.mesh.edges[adj1].nextIdx;
        }
        */
    return -1;
}