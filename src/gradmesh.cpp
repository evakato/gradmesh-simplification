#include "gradmesh.hpp"

std::optional<std::vector<Patch>> GradMesh::generatePatches() const
{
    std::vector<Patch> patches{};

    for (int faceIdx = 0; faceIdx < faces.size(); faceIdx++)
    {
        const auto &face = faces[faceIdx];
        if (!face.isValid())
            continue;

        auto [e0, e1, e2, e3] = getFaceEdgeIdxs(face.halfEdgeIdx);
        auto topEdgeDerivatives = computeEdgeDerivatives(edges[e0]);
        auto rightEdgeDerivatives = computeEdgeDerivatives(edges[e1]);
        auto bottomEdgeDerivatives = computeEdgeDerivatives(edges[e2]);
        auto leftEdgeDerivatives = computeEdgeDerivatives(edges[e3]);

        if (!topEdgeDerivatives || !rightEdgeDerivatives || !bottomEdgeDerivatives || !leftEdgeDerivatives)
            return std::nullopt;

        auto [m0, m0v, m1v, m0uv] = topEdgeDerivatives.value();
        auto [m1, m1u, m3u, m1uv] = rightEdgeDerivatives.value();
        auto [m3, m3v, m2v, m3uv] = bottomEdgeDerivatives.value();
        auto [m2, m2u, m0u, m2uv] = leftEdgeDerivatives.value();

        std::vector<Vertex> controlMatrix = {m0, m0v, m1v, m1,       //
                                             -m0u, m0uv, -m1uv, m1u, //
                                             -m2u, -m2uv, m3uv, m3u, //
                                             m2, -m2v, -m3v, m3};

        patches.push_back(Patch{controlMatrix, faceIdx, {e0, e1, e2, e3}});
    }

    return patches;
}

EdgeDerivatives GradMesh::getCurve(int halfEdgeIdx, int depth) const
{
    assert(halfEdgeIdx != -1);
    const auto &edge = edges[halfEdgeIdx];

    if (!edges[halfEdgeIdx].isValid())
    {
        std::cerr << "half edge idx not valid: " << halfEdgeIdx << std::endl;
        assert(false);
    }
    assert(edges[edge.nextIdx].isValid());

    auto edgeDerivatives = computeEdgeDerivatives(edge, ++depth);
    if (!edgeDerivatives)
        return std::nullopt;

    auto [m0, m0v, m1v, m0uv] = edgeDerivatives.value();

    auto nextEdgeDerivatives = computeEdgeDerivatives(edges[edge.nextIdx], ++depth);
    if (!nextEdgeDerivatives)
        return std::nullopt;

    auto m1 = nextEdgeDerivatives.value()[0];
    return CurveVector{m0, m0v, m1v, m1};
}

EdgeDerivatives GradMesh::computeEdgeDerivatives(const HalfEdge &edge, int depth) const
{
    if (depth > MAX_CURVE_DEPTH)
    {
        return std::nullopt;
        // assert(false && "Maximum recursion depth exceeded in functionA!");
    }

    std::array<Vertex, 4> edgeDerivatives;

    if (!edge.isChild())
    {
        if (edge.originIdx == -1)
        {
            std::cout << edge << std::endl;
            assert(false);
        }
        assert(edge.handleIdxs.first != -1 && edge.handleIdxs.second != -1);

        edgeDerivatives[0] = Vertex{points[edge.originIdx].coords, edge.color};
        edgeDerivatives[1] = Vertex{handles[edge.handleIdxs.first]};
        edgeDerivatives[2] = -Vertex{handles[edge.handleIdxs.second]};
    }
    else
    {
        assert(edge.interval[0] >= 0 && edge.interval[0] <= 1 && edge.interval[1] >= 0 && edge.interval[1] <= 1);

        auto parentCurve = getCurve(edge.parentIdx, ++depth);
        if (!parentCurve)
            return std::nullopt;

        Vertex v = interpolateCubic(parentCurve.value(), edge.interval[0]);
        edgeDerivatives[0] = Vertex{v.coords, edge.color};
        if (edge.isBar())
        {
            float scale = edge.interval[1] - edge.interval[0];
            edgeDerivatives[1] = interpolateCubicDerivative(parentCurve.value(), edge.interval[0]) * scale;
            edgeDerivatives[2] = interpolateCubicDerivative(parentCurve.value(), edge.interval[1]) * scale;
        }
        else if (edge.isStem())
        {
            assert(edge.handleIdxs.first != -1 && edge.handleIdxs.second != -1);
            edgeDerivatives[1] = Vertex{handles[edge.handleIdxs.first]};
            edgeDerivatives[2] = -Vertex{handles[edge.handleIdxs.second]};
        }
    }
    edgeDerivatives[3] = edge.twist;

    return edgeDerivatives;
}

std::vector<Vertex> GradMesh::getHandleBars() const
{
    std::vector<Vertex> pointsAndHandles = {};
    for (auto &edge : edges)
    {
        if (edge.faceIdx != -1 && !edge.isBar())
        {
            assert(edge.handleIdxs.first != -1 && edge.handleIdxs.second != -1);

            auto &point = computeEdgeDerivatives(edge).value()[0];
            auto &tangent = handles[edge.handleIdxs.first];
            pointsAndHandles.push_back(Vertex{point.coords, black});
            pointsAndHandles.push_back(Vertex{hermiteToBezier(point.coords, BCM, tangent.coords), black});
            auto &m1 = computeEdgeDerivatives(edges[edge.nextIdx]).value()[0];
            auto &m1v = handles[edge.handleIdxs.second];
            pointsAndHandles.push_back(Vertex{m1.coords, black});
            pointsAndHandles.push_back(Vertex{hermiteToBezier(m1.coords, BCM, m1v.coords), black});
        }
    }
    return pointsAndHandles;
}

std::vector<Vertex> GradMesh::getControlPoints() const
{
    std::vector<Vertex> controlPoints = {};
    for (auto &point : points)
        if (point.isValid())
            controlPoints.push_back(Vertex{point.coords, white});
    return controlPoints;
}

void GradMesh::fixEdges()
{
    int count = 0;
    int halfEdgeIdx = 0;
    for (HalfEdge &edge : edges)
    {
        if (edge.parentIdx != -1 && approximateFloating(edge.interval[0]) && edge.interval[1] == 1)
        {
            // likely a bandaid fix for certain .hemesh files.. ideally i want to get rid of all those annoying parent edges completely, but for now this will do
            HalfEdge &parent = edges[edge.parentIdx];
            parent.childIdxDegenerate = halfEdgeIdx;
            parent.faceIdx = -1;
            edge.originIdx = parent.originIdx;
            edge.handleIdxs = parent.handleIdxs;
            edge.parentIdx = -1;
            count++;
        }
        halfEdgeIdx++;
    }
    for (Face &face : faces)
    {
        if (face.halfEdgeIdx == -1)
            continue;
        int edgeIdx = face.halfEdgeIdx;
        for (int i = 0; i < 4; i++)
        {
            auto &edge = edges[edgeIdx];
            if (edge.twinIdx != -1 && edges[edge.twinIdx].childIdxDegenerate != -1)
                edge.twinIdx = edges[edge.twinIdx].childIdxDegenerate;
            // std::cout << "why is the twin: " << edge.twinIdx << std::endl;
            edgeIdx = edge.nextIdx;
        }
    }
    // std::cout << "There are " << count << " weird edges.\n";
}

std::array<int, 4> GradMesh::getFaceEdgeIdxs(int edgeIdx) const
{
    std::array<int, 4> edgeIdxs;
    int currIdx = edgeIdx;
    for (int i = 0; i < 4; i++)
    {
        auto &currEdge = edges[currIdx];
        edgeIdxs[i] = currIdx;
        currIdx = currEdge.nextIdx;
    }
    return edgeIdxs;
}

AABB GradMesh::getFaceAABB(int halfEdgeIdx) const
{
    AABB aabb{glm::vec2(std::numeric_limits<float>::max()), glm::vec2(std::numeric_limits<float>::lowest())};
    auto edgeIdxs = getFaceEdgeIdxs(halfEdgeIdx);
    for (int edgeIdx : edgeIdxs)
    {
        auto curveVector = getCurve(edgeIdx);
        assert(curveVector != std::nullopt);
        auto curve = curveVector.value();
        Curve newCurve = Curve{curve[0], curve[1], curve[2], curve[3], Curve::CurveType::Hermite};
        aabb.expand(newCurve.getAABB());
    }
    return aabb;
}

AABB GradMesh::getMeshAABB() const
{
    AABB aabb{};
    for (auto &face : faces)
        if (face.isValid())
            aabb.expand(getFaceAABB(face.halfEdgeIdx));
    return aabb;
}

AABB GradMesh::getAffectedMergeAABB(int halfEdgeIdx) const
{
    AABB aabb;
    auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = getFaceEdgeIdxs(halfEdgeIdx);
    auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = getFaceEdgeIdxs(getTwinIdx(halfEdgeIdx));
    int twin1 = getTwinIdx(face1BIdx);
    int twin2 = getTwinIdx(face1TIdx);
    int twin3 = getTwinIdx(face2BIdx);
    int twin4 = getTwinIdx(face2TIdx);
    for (int eIdx : {halfEdgeIdx, face2LIdx, twin1, twin2, twin3, twin4})
    {
        int currIdx = eIdx;
        if (eIdx == -1)
            continue;
        if (!edges[currIdx].isValid())
            continue;
        while (true)
        {
            aabb.expand(getFaceAABB(currIdx));
            auto &e = edges[currIdx];
            if (!e.isChild())
                break;
            currIdx = e.parentIdx;
        }
    }
    return aabb;
}

std::vector<std::pair<int, int>> GradMesh::findCornerFaces() const
{
    std::vector<std::pair<int, int>> cornerFaces;

    for (const auto &face : faces)
    {
        if (!face.isValid())
            continue;

        auto faceEdges = getFaceEdgeIdxs(face.halfEdgeIdx);
        for (size_t i = 0; i < faceEdges.size(); ++i)
        {
            int currentIdx = faceEdges[i];
            int nextIdx = faceEdges[(i + 1) % faceEdges.size()];
            int thirdIdx = faceEdges[(i + 2) % faceEdges.size()];
            if (edges[currentIdx].twinIdx == -1 && edges[nextIdx].twinIdx == -1)
            {
                if (edges[thirdIdx].twinIdx == -1)
                {
                    cornerFaces.push_back({faceEdges[(i + 3) % faceEdges.size()], -1});
                    std::cout << "een\n";
                }
                std::array<int, 2> nonConsecutiveEdges{};
                int count = 0;
                for (size_t loopIdx = 0; loopIdx < faceEdges.size(); ++loopIdx)
                {
                    int idx = faceEdges[loopIdx];

                    if (idx != currentIdx && idx != nextIdx && edges[idx].twinIdx != -1)
                    {
                        nonConsecutiveEdges[count++] = idx;
                        if (count == 2 && loopIdx != 3)
                            cornerFaces.push_back({nonConsecutiveEdges[0], nonConsecutiveEdges[1]});
                        else if (count == 2)
                            cornerFaces.push_back({nonConsecutiveEdges[1], nonConsecutiveEdges[0]});
                    }
                }
            }
        }
    }
    return cornerFaces;
}

void GradMesh::findULPoints()
{
    std::map<int, std::pair<int, int>> pointMap;
    for (size_t edgeIdx = 0; edgeIdx < edges.size(); edgeIdx++)
    {
        auto &edge = edges[edgeIdx];
        if (!edge.isValid() || edge.isChild())
            continue;

        auto it = pointMap.find(edge.originIdx);
        if (it != pointMap.end())
        {
            ++pointMap[edge.originIdx].first;
        }
        else
            pointMap[edge.originIdx] = {1, 0};

        if (!edge.hasTwin())
            ++pointMap[edge.originIdx].second;
    }
    for (const auto &point : pointMap)
    {
        auto pair = point.second;
        if (pair.first > 2 && pair.second > 0)
            ulPointIdxs.push_back(point.first);
    }
}

bool GradMesh::isULMergeEdge(const HalfEdge &edge) const
{
    if (std::ranges::find(ulPointIdxs, edge.originIdx) != ulPointIdxs.end())
    {
        return true;
    }
    if (edge.hasTwin())
    {
        const auto &twinEdge = edges[edge.twinIdx];
        if (std::ranges::find(ulPointIdxs, twinEdge.originIdx) != ulPointIdxs.end())
        {
            return true;
        }
    }
    return false;
}

std::vector<std::pair<int, int>> GradMesh::getGridEdgeIdxs(int rowIdx, int colIdx) const
{
    std::vector<std::pair<int, int>> gridEdgeIdxs;
    int currRowIdx = rowIdx;
    int currColIdx = colIdx;

    auto getGridIdxsInRow = [&](int startIdx, int rowLength)
    {
        int currIdx = startIdx;
        int i = 0;
        while (i < rowLength)
        {
            const auto &currEdge = edges[currIdx];
            gridEdgeIdxs.push_back({currIdx, currEdge.nextIdx});
            if (currEdge.twinIdx == -1)
                return;
            currIdx = edges[edges[currEdge.twinIdx].nextIdx].nextIdx;
            ++i;
        }
    };

    getGridIdxsInRow(currRowIdx, std::numeric_limits<int>::max());
    int rowLength = gridEdgeIdxs.size();
    while (true)
    {
        const auto &colTwinIdx = edges[currColIdx].twinIdx;
        if (colTwinIdx == -1)
            break;

        currRowIdx = edges[colTwinIdx].nextIdx;
        currColIdx = edges[currRowIdx].nextIdx;
        getGridIdxsInRow(currRowIdx, rowLength);
    }

    return gridEdgeIdxs;
}

AABB GradMesh::getProductRegionAABB(const std::pair<int, int> &gridPair, const std::pair<int, int> &maxRegion) const
{
    AABB aabb;
    aabb.expand(getFaceAABB(gridPair.first));

    int currIdx = gridPair.first;
    int currIdx2 = gridPair.second;
    for (int j = 0; j <= maxRegion.second; j++)
    {
        aabb.expand(getFaceAABB(currIdx2));

        for (int i = 0; i < maxRegion.first; i++)
        {
            currIdx = edges[edges[edges[currIdx].twinIdx].nextIdx].nextIdx;
            aabb.expand(getFaceAABB(currIdx));
        }
        const auto &currIdx2Twin = edges[currIdx2].twinIdx;
        if (currIdx2Twin == -1)
            break;

        currIdx = edges[currIdx2Twin].nextIdx;
        currIdx2 = edges[currIdx].nextIdx;
    }
    return aabb;
}

std::vector<int> GradMesh::getIncidentFacesOfRegion(const Region &region) const
{
    auto gridPair = region[0];
    auto maxRegion = region[1];
    std::vector<int> faceIdxs;
    faceIdxs.push_back(edges[gridPair.first].faceIdx);

    int currIdx = gridPair.first;
    int currIdx2 = gridPair.second;
    for (int j = 0; j <= maxRegion.second; j++)
    {
        faceIdxs.push_back(edges[currIdx2].faceIdx);

        for (int i = 0; i < maxRegion.first; i++)
        {
            currIdx = edges[edges[edges[currIdx].twinIdx].nextIdx].nextIdx;
            faceIdxs.push_back(edges[currIdx].faceIdx);
        }
        const auto &currIdx2Twin = edges[currIdx2].twinIdx;
        if (currIdx2Twin == -1)
            break;

        currIdx = edges[currIdx2Twin].nextIdx;
        currIdx2 = edges[currIdx].nextIdx;
    }
    return faceIdxs;
}

bool GradMesh::regionsOverlap(const Region &region1, const Region &region2) const
{
    auto faces1 = getIncidentFacesOfRegion(region1);
    auto faces2 = getIncidentFacesOfRegion(region2);
    return std::ranges::any_of(faces1, [&faces2](int val)
                               { return std::ranges::find(faces2, val) != faces2.end(); });
}

int GradMesh::maxDependencyChain() const
{
    int maxChain = 0;
    for (auto &edge : edges)
    {
        if (!edge.isValid() || !edge.isChild())
            continue;

        int newChain = 0;
        auto currEdge = edge;
        while (true)
        {
            if (currEdge.parentIdx == -1)
                break;
            newChain++;
            currEdge = edges[currEdge.parentIdx];
        }
        maxChain = std::max(maxChain, newChain);
    }
    return maxChain;
}

int GradMesh::getNextRowIdx(int halfEdgeIdx) const
{
    if (halfEdgeIdx == -1)
        return -1;
    auto &e = edges[halfEdgeIdx];
    if (!e.isValid())
        return -1;
    auto &nextE = edges[e.nextIdx];
    if (!nextE.isValid())
        return -1;
    int orthoIdx = nextE.twinIdx;
    if (orthoIdx == -1)
        return -1;
    auto &orthoE = edges[orthoIdx];
    if (!orthoE.isValid())
        return -1;
    return orthoE.nextIdx;
}