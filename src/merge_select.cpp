#include "merge_select.hpp"
#include <random>

MergeSelect::MergeSelect(GmsAppState &state) : state(state) {}

int randomCorner(int N)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, N);
    return distrib(gen);
}

int MergeSelect::validCorners()
{
    std::vector<int> validCorners;
    for (int i = 0; i < cornerFaces.size(); i++)
    {
        if (std::find(seenCorners.begin(), seenCorners.end(), i) != seenCorners.end())
        {
            continue;
        }
        auto &cornerPairs = cornerFaces[i];
        if (state.mesh.validMergeEdge(cornerPairs.first) && cornerPairs.second == -1)
        {
            validCorners.push_back(i);
        }
        else if (state.mesh.validMergeEdge(cornerPairs.first) && state.mesh.validMergeEdge(cornerPairs.second))
        {
            validCorners.push_back(i);
        }
    }
    std::cout << "hello? " << validCorners.size() << std::endl;
    if (validCorners.size() <= 3 || seenCorners.size() > 3)
        return -1;
    int rCorner = randomCorner(validCorners.size() - 1);
    seenCorners.push_back(validCorners[rCorner]);
    return validCorners[rCorner];
}

void MergeSelect::setCornerEdges()
{
    int idx = validCorners();
    if (idx == -1)
        return;
    cornerEdges = cornerFaces[idx];
    int secondEdge = cornerEdges.second;
    if (secondEdge == -1)
        return;
    if (state.mesh.edges[secondEdge].nextIdx == cornerEdges.first)
    {
        cornerEdges.second = cornerEdges.first;
        cornerEdges.first = secondEdge;
    }
}

void MergeSelect::setCurrAdjPair()
{
    auto [adj1, adj2] = cornerEdges;
    int adj2Twin = adj2 == -1 ? -1 : state.mesh.getTwinIdx(adj2);
    int nextEdgeIdx = adj2Twin == -1 ? -1 : state.mesh.edges[adj2Twin].nextIdx;
    currAdjPair = {adj1, nextEdgeIdx};
    otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);
}

void MergeSelect::reset()
{
    otherDirEdges.clear();
    seenFailedEdges.clear();
    cornerFaces = state.mesh.findCornerFaces();

    setCornerEdges();
    setCurrAdjPair();

    otherDirIdx = 0;
    verticalDir = false;
    firstRow = true;
}

void MergeSelect::findCandidateMerges(std::vector<SingleHalfEdge> *boundaryEdges)
{
    auto &candidateMerges = state.candidateMerges;
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
                if (mesh.validMergeEdge(currEdge))
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
                else if (boundaryEdges)
                {
                    boundaryEdges->push_back(SingleHalfEdge{CurveId{faceIdx, i}, currIdx});
                }
                currIdx = currEdge.nextIdx;
            }
            faceIdx++;
        }
    }
}

int MergeSelect::selectEdge()
{
    switch (state.mergeMode)
    {
    case MANUAL:
        return state.candidateMerges[state.selectedEdgeId].getHalfEdgeIdx();
    case RANDOM:
        return selectRandomEdge();
    case GRID:
        return selectGridEdge();
    case DUAL_GRID:
        return selectDualGridEdge();
    }
    return -1;
}

int MergeSelect::selectRandomEdge()
{
    switch (state.mergeStatus)
    {
    case NA:
    case SUCCESS:
    {
        selectedEdgePool = generateRandomNums(state.candidateMerges.size() - 1);
        state.attemptedMergesIdx = 0;
        break;
    }
    case CYCLE:
    case METRIC_ERROR:
    {
        if (state.attemptedMergesIdx >= selectedEdgePool.size())
        {
            state.mergeMode = NONE;
            return -1;
        }
        break;
    }
    }
    state.selectedEdgeId = selectedEdgePool[state.attemptedMergesIdx++];
    return state.candidateMerges[state.selectedEdgeId].getHalfEdgeIdx();
}

int MergeSelect::selectVerticalGridEdge()
{
    int currIdx = otherDirEdges[otherDirIdx];
    if (state.mergeStatus != SUCCESS && state.mergeStatus != NA)
    {
        otherDirIdx++;
    }

    const auto *currEdge = &state.mesh.edges[currIdx];
    while (!currEdge->isValid() || !state.mesh.validMergeEdge(*currEdge))
    {
        otherDirIdx++;
        if (otherDirIdx >= otherDirEdges.size())
        {
            otherDirEdges.clear();
            verticalDir = false;
            otherDirIdx = 0;

            if (validCorners() == -1)
                return -1;

            setCornerEdges();
            setCurrAdjPair();
            return currAdjPair.first;
        }
        currIdx = otherDirEdges[otherDirIdx];
        currEdge = &state.mesh.edges[currIdx];
    }
    return currIdx;
}

int MergeSelect::selectGridEdge()
{
    if (verticalDir)
        return selectVerticalGridEdge();

    auto [adj1, adj2] = currAdjPair;
    if (state.mergeStatus != SUCCESS && state.mergeStatus != NA)
    {
        if (std::find(seenFailedEdges.begin(), seenFailedEdges.end(), adj1) != seenFailedEdges.end())
        {
            verticalDir = true;
            return selectVerticalGridEdge();
        }
        seenFailedEdges.push_back(adj1);
        if (state.mesh.getTwinIdx(adj1) != -1)
        {
            adj1 = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(adj1))[2];
            currAdjPair.first = adj1;
            otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);
        }
    }
    const auto &currEdge = state.mesh.edges[adj1];
    if (!currEdge.hasTwin())
    {
        if (adj2 == -1)
        {
            verticalDir = true;
            return selectVerticalGridEdge();
        }
        int nextAdj2 = state.mesh.edges[adj2].nextIdx;
        int adj2Twin = state.mesh.getTwinIdx(nextAdj2);
        int nextEdgeIdx = (adj2Twin == -1 || state.mesh.edges[nextAdj2].isBar()) ? -1 : state.mesh.edges[adj2Twin].nextIdx;
        currAdjPair = {adj2, nextEdgeIdx};
        otherDirEdges.push_back(state.mesh.edges[adj2].nextIdx);
        return adj2;
    }
    return adj1;
}

int MergeSelect::selectDualGridEdge()
{
    if (state.numOfMerges == 0 && state.attemptedMergesIdx == 0)
    {
        otherDirEdges.clear();
        auto [adj1, adj2] = cornerEdges;
        currAdjPair = {adj1, adj2};
        state.attemptedMergesIdx++;
        return adj1;
    }
    if (verticalDir)
        return selectVerticalGridEdge();

    auto [adj1, adj2] = currAdjPair;

    if (state.mergeStatus == SUCCESS)
    {
        if (firstRow)
        {
            currAdjPair.second = adj1;
            firstRow = false;
        }
        auto [e1, e2, e3, e4] = state.mesh.getFaceEdgeIdxs(adj1);
        int tBar1 = state.mesh.getTwinIdx(e2);
        if (tBar1 != -1)
        {
            auto &tParent1 = state.mesh.edges[tBar1];

            int rightMostStem = -1;
            float maxInterval = 0.0f;
            for (int childIdx : tParent1.childrenIdxs)
            {
                if (state.mesh.edges[childIdx].isStem() && state.mesh.edges[childIdx].interval.x > maxInterval)
                {
                    maxInterval = state.mesh.edges[childIdx].interval.x;
                    rightMostStem = childIdx;
                }
            }

            currAdjPair.first = rightMostStem;
            otherDirEdges.push_back(state.mesh.edges[rightMostStem].nextIdx);
            return rightMostStem;
        }
    }
    else if (state.mergeStatus == METRIC_ERROR || state.mergeStatus == CYCLE)
    {
        if (std::find(seenFailedEdges.begin(), seenFailedEdges.end(), adj1) != seenFailedEdges.end())
        {
            verticalDir = true;
            return selectVerticalGridEdge();
        }
        seenFailedEdges.push_back(adj1);
        if (firstRow)
        {
            int twinMergeEdge = state.mesh.getTwinIdx(adj1);
            if (twinMergeEdge == -1)
                currAdjPair.second = -1;
            else
            {
                auto [e1f, e2f, e3f, e4f] = state.mesh.getFaceEdgeIdxs(twinMergeEdge);
                currAdjPair.second = e3f;
            }
            firstRow = false;
        }
        auto [e1, e2, e3, e4] = state.mesh.getFaceEdgeIdxs(adj1);
        auto [e1t, e2t, e3t, e4t] = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(adj1));
        otherDirEdges.push_back(e2);
        otherDirEdges.push_back(e4t);

        auto &topEdge = state.mesh.edges[e2];
        int twinIdx = state.mesh.getTwinIdx(e2);
        if (topEdge.isBar())
            twinIdx = state.mesh.getTwinIdx(topEdge.parentIdx); // sigh the twins are still imperfect
        if (twinIdx != -1)
        {
            int newAdj1 = state.mesh.edges[twinIdx].nextIdx;
            currAdjPair.first = newAdj1;
            return newAdj1;
        }
    }
    if (state.mesh.getTwinIdx(adj2) == -1)
    {
        verticalDir = true;
        return selectVerticalGridEdge();
    }
    firstRow = true;
    otherDirEdges.push_back(state.mesh.edges[adj2].nextIdx);
    currAdjPair.first = adj2;
    return adj2;
}