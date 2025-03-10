#include "merge_select.hpp"
#include <random>
#include <ctime>
#include <chrono>

MergeSelect::MergeSelect(GmsAppState &state) : state(state) {}

// Proper seeding using multiple entropy sources
std::mt19937 createRandomGenerator()
{
    auto timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::seed_seq seedSeq{static_cast<unsigned>(timeSeed), rd(), rd(), rd(), rd()};
    return std::mt19937(seedSeq);
}

std::mt19937 gen = createRandomGenerator(); // Global RNG

int randomCorner(int N)
{
    std::uniform_int_distribution<> distrib(0, N - 1);
    return distrib(gen);
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
    seenCornerFaces.clear();
    cornerFaces.clear();

    cornerFaces = state.mesh.findCornerFaces();
    for (auto &cornerFace : cornerFaces)
    {
        // std::cout << cornerFace.first << ", " << cornerFace.second << std::endl;
    }
    cornerEdges = cornerFaces[randomCorner(cornerFaces.size())];
    seenCornerFaces.push_back(cornerEdges);
    setCurrAdjPair();
    detectOverlappingCorners();

    int j = 0;
    while (cornerFaces.size() > seenCornerFaces.size())
    {
        ++j;
        if (j > 100)
            break;
        for (int i = 0; i < 10; i++)
        {
            int randthing = randomCorner(cornerFaces.size());
            auto newCorner = cornerFaces[randthing];
            if (std::find(seenCornerFaces.begin(), seenCornerFaces.end(), newCorner) == seenCornerFaces.end())
            {
                seenCornerFaces.push_back(newCorner);
                detectOverlappingCorners();
                break;
            }
        }
    }

    std::cout << cornerFaces.size() << std::endl;
    std::cout << seenCornerFaces.size() << std::endl;

    otherDirIdx = 0;
    verticalDir = false;
    firstRow = true;
}

void MergeSelect::detectOverlappingCorners()
{
    std::pair<int, int> tempAdjPair = {currAdjPair.first, currAdjPair.second};
    int currIdx = tempAdjPair.first;
    while (true)
    {
        const auto &currEdge = state.mesh.edges[currIdx];
        if (!currEdge.hasTwin())
        {
            auto idxs = state.mesh.getFaceEdgeIdxs(currIdx);
            const auto &bottomEdge = state.mesh.edges[idxs[3]];
            const auto &topEdge = state.mesh.edges[idxs[1]];
            if (!bottomEdge.hasTwin() || !topEdge.hasTwin())
            {
                for (int i = 0; i < cornerFaces.size(); i++)
                {
                    if (std::find(idxs.begin(), idxs.end(), cornerFaces[i].first) != idxs.end() || std::find(idxs.begin(), idxs.end(), cornerFaces[i].second) != idxs.end())
                    {
                        cornerFaces.erase(cornerFaces.begin() + i);
                        break;
                    }
                }
            }

            if (tempAdjPair.second == -1)
            {
                auto idxs = state.mesh.getFaceEdgeIdxs(tempAdjPair.first);
                for (int i = 0; i < cornerFaces.size(); i++)
                {
                    if (std::find(idxs.begin(), idxs.end(), cornerFaces[i].first) != idxs.end() || std::find(idxs.begin(), idxs.end(), cornerFaces[i].second) != idxs.end())
                    {
                        cornerFaces.erase(cornerFaces.begin() + i);
                        break;
                    }
                }
                break;
            }

            auto nextFaceEdges = state.mesh.getFaceEdgeIdxs(tempAdjPair.second);
            tempAdjPair.first = nextFaceEdges[0];
            const auto &nextEdge = state.mesh.edges[nextFaceEdges[1]];
            if (nextEdge.hasTwin())
            {
                tempAdjPair.second = state.mesh.edges[nextEdge.twinIdx].nextIdx;
            }
            else
            {
                tempAdjPair.second = -1;
            }
            currIdx = tempAdjPair.first;
        }
        else
        {
            currIdx = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(currIdx))[2];
        }
    }
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

            ++currCornerFaceIdx;
            if (currCornerFaceIdx >= seenCornerFaces.size())
                return -1;

            cornerEdges = seenCornerFaces[currCornerFaceIdx];
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