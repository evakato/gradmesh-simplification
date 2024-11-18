#include "merge_select.hpp"

MergeSelect::MergeSelect(GmsAppState &state) : state(state) {}

void MergeSelect::reset()
{
    otherDirEdges.clear();
    auto cornerFaces = state.mesh.findCornerFace();
    std::cout << "Setting new corner " << cornerFaces.size() << std::endl;
    cornerEdges = cornerFaces[0];

    auto [adj1, adj2] = cornerEdges;
    int adj2Twin = adj2 == -1 ? -1 : state.mesh.getTwinIdx(adj2);
    int nextEdgeIdx = adj2Twin == -1 ? -1 : state.mesh.edges[adj2Twin].nextIdx;
    std::cout << "Curr adj pair: " << adj1 << ", " << nextEdgeIdx << std::endl;
    currAdjPair = {adj1, nextEdgeIdx};
    otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);

    // currAdjPair = {-1, -1};
    otherDirIdx = 0;
    verticalDir = false;
    firstRow = true;
    sortedRegionsIdx = 0;
}

void MergeSelect::findCandidateMerges(std::vector<CurveId> *boundaryEdges)
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
                if (validEdgeType(currEdge))
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
                    boundaryEdges->push_back(CurveId{faceIdx, i});
                }
                currIdx = currEdge.nextIdx;
            }
            faceIdx++;
        }
    }
}

void MergeSelect::preprocess()
{
    sortedRegions = state.edgeRegions;
    std::ranges::sort(sortedRegions, std::ranges::greater{}, &EdgeRegion::getMaxPatches);
    sortedRegionsIdx = 0;
}

int MergeSelect::selectEdge()
{
    if (state.usePreprocessing && selectNewRegion)
    {
        auto &selectedRegion = sortedRegions[sortedRegionsIdx++];
        if (selectedRegion.getMaxPatches() == 1)
            return -1;

        auto [adj1, adj2] = selectedRegion.gridPair;
        maxRegion = selectedRegion.maxRegion;
        int adj2Twin = adj2 == -1 ? -1 : state.mesh.getTwinIdx(adj2);
        int nextEdgeIdx = adj2Twin == -1 ? -1 : state.mesh.edges[adj2Twin].nextIdx;
        currAdjPair = {adj1, nextEdgeIdx};
        otherDirEdges.clear();
        otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);
        selectNewRegion = false;
        currGridIdxs = {0, 0};
    }
    else if (selectNewRegion)
    {
        auto [adj1, adj2] = cornerEdges;
        int adj2Twin = adj2 == -1 ? -1 : state.mesh.getTwinIdx(adj2);
        int nextEdgeIdx = adj2Twin == -1 ? -1 : state.mesh.edges[adj2Twin].nextIdx;
        currAdjPair = {adj1, nextEdgeIdx};
        otherDirEdges.clear();
        otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);
        selectNewRegion = false;
        currGridIdxs = {0, 0};
        selectNewRegion = false;
        maxRegion = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    }

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
    while (!currEdge->isValid() || !validEdgeType(*currEdge))
    {
        otherDirIdx++;
        if (otherDirIdx >= otherDirEdges.size())
        {
            otherDirEdges.clear();
            verticalDir = false;
            selectNewRegion = true;
            return -1;
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

    currGridIdxs.first++;
    auto [adj1, adj2] = currAdjPair;
    if (state.mergeStatus != SUCCESS && state.mergeStatus != NA)
    {
        if (state.mesh.getTwinIdx(adj1) != -1)
        {
            adj1 = state.mesh.getFaceEdgeIdxs(state.mesh.getTwinIdx(adj1))[2];
            currAdjPair.first = adj1;
            otherDirEdges.push_back(state.mesh.edges[adj1].nextIdx);
        }
    }
    const auto &currEdge = state.mesh.edges[adj1];
    if (!currEdge.hasTwin() || currGridIdxs.first > maxRegion.first)
    {
        if (adj2 == -1)
        {
            // change grid to other direction
            verticalDir = true;
            return selectVerticalGridEdge();
        }
        int nextAdj2 = state.mesh.edges[adj2].nextIdx;
        int adj2Twin = state.mesh.getTwinIdx(nextAdj2);
        int nextEdgeIdx = (adj2Twin == -1 || state.mesh.edges[nextAdj2].isBar()) ? -1 : state.mesh.edges[adj2Twin].nextIdx;
        currAdjPair = {adj2, nextEdgeIdx};
        otherDirEdges.push_back(state.mesh.edges[adj2].nextIdx);
        currGridIdxs.first = 0;
        return adj2;
    }
    return adj1;
}

bool MergeSelect::validEdgeType(const HalfEdge &edge) const
{
    return (edge.hasTwin() && !edge.isBar() && !state.mesh.twinIsParent(edge) && !edge.isParent() && !state.mesh.isULMergeEdge(edge));
}

bool MergeSelect::checkCycle(int edgeIdx) const
{
    auto &edge = state.mesh.edges[edgeIdx];
    std::cout << "edgeIdx: " << edgeIdx << ", edge origin: " << edge.originIdx << "\n";

    auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = state.mesh.getFaceEdgeIdxs(edgeIdx);
    std::cout << "face1 edges - R: " << face1RIdx << ", B: " << face1BIdx
              << ", L: " << face1LIdx << ", T: " << face1TIdx << "\n";

    if (edge.twinIdx == -1)
    {
        std::cout << "No twin for edge, returning false.\n";
        return false;
    }

    auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = state.mesh.getFaceEdgeIdxs(edge.twinIdx);
    std::cout << "face2 edges - L: " << face2LIdx << ", T: " << face2TIdx
              << ", R: " << face2RIdx << ", B: " << face2BIdx << "\n";

    int face1TopTwin = state.mesh.getTwinIdx(face1TIdx);
    std::cout << "face1 top twin index: " << face1TopTwin << "\n";

    int topLeft = face1TopTwin == -1 ? -1 : state.mesh.getFaceEdgeIdxs(face1TopTwin)[3];
    std::cout << "topLeft: " << topLeft << "\n";

    int topRight = state.mesh.getTwinIdx(face2TIdx);
    std::cout << "topRight: " << topRight << "\n";

    int face2BottomTwin = state.mesh.getTwinIdx(face2BIdx);
    std::cout << "face2 bottom twin index: " << face2BottomTwin << "\n";

    int bottomLeft = state.mesh.getTwinIdx(face1BIdx);
    std::cout << "bottomLeft: " << bottomLeft << "\n";

    int bottomRight = face2BottomTwin == -1 ? -1 : state.mesh.getFaceEdgeIdxs(face2BottomTwin)[3];
    std::cout << "bottomRight: " << bottomRight << "\n";

    auto &twinEdge = state.mesh.edges[face2LIdx];
    std::cout << "twinEdge origin index: " << twinEdge.originIdx << "\n";

    bool result = (bottomRight == -1 ? false : state.mesh.findParentPointIdx(bottomRight) == twinEdge.originIdx //
                                                   || bottomLeft == -1
                                               ? false
                                           : state.mesh.findParentPointIdx(bottomLeft) == twinEdge.originIdx //
                                                   || topLeft == -1
                                               ? false
                                           : state.mesh.findParentPointIdx(topLeft) == edge.originIdx //
                                                   || topRight == -1
                                               ? false
                                               : state.mesh.findParentPointIdx(topRight) == edge.originIdx);

    std::cout << "Final result: " << result << "\n";
    return result;
}

int MergeSelect::selectDualGridEdge()
{
    if (verticalDir)
    {
        int currIdx = otherDirEdges[otherDirIdx];
        if (state.mergeStatus != SUCCESS && state.mergeStatus != NA)
        {
            otherDirIdx++;
        }

        const auto *currEdge = &state.mesh.edges[currIdx];
        while (!currEdge->isValid() || !validEdgeType(*currEdge))
        {
            otherDirIdx++;
            if (otherDirIdx >= otherDirEdges.size())
                return -1;
            currIdx = otherDirEdges[otherDirIdx];
            currEdge = &state.mesh.edges[currIdx];
        }
        return currIdx;
    }

    if (cornerEdges == std::make_pair(-1, -1))
    {
        auto cornerFaces = state.mesh.findCornerFace();
        std::cout << cornerFaces.size() << std::endl;
        cornerEdges = cornerFaces[0];
        auto [adj1, adj2] = cornerEdges;
        currAdjPair = {adj1, adj2};
        std::cout << "finding corner: " << adj1 << std::endl;
        return adj1;
    }

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
        return -1;
    }
    firstRow = true;
    otherDirEdges.push_back(state.mesh.edges[adj2].nextIdx);
    currAdjPair.first = adj2;
    return adj2;
}