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
        assert(edge.originIdx != -1);
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
            // std::cout << "the child is " << halfEdgeIdx << "and the parent is " << edge.parentIdx << " who belongs to " << parent.faceIdx << "\n";
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

AABB GradMesh::getFaceBoundingBox(int halfEdgeIdx) const
{
    AABB aabb{glm::vec2(std::numeric_limits<float>::max()), glm::vec2(std::numeric_limits<float>::lowest())};
    auto edgeIdxs = getFaceEdgeIdxs(halfEdgeIdx);
    for (int edgeIdx : edgeIdxs)
    {
        while (true)
        {
            auto curveVector = getCurve(edgeIdx);
            assert(curveVector != std::nullopt);
            auto curve = curveVector.value();
            Curve newCurve = Curve{curve[0], curve[1], curve[2], curve[3], Curve::CurveType::Bezier};
            aabb.expand(newCurve.getAABB());

            if (!edges[edgeIdx].isChild())
                break;

            edgeIdx = edges[edgeIdx].parentIdx;
        }
    }
    return aabb;
}

AABB GradMesh::getBoundingBoxOverFaces(std::vector<int> halfEdgeIdxs) const
{
    AABB aabb{glm::vec2(std::numeric_limits<float>::max()), glm::vec2(std::numeric_limits<float>::lowest())};

    for (int halfEdgeIdx : halfEdgeIdxs)
    {
        if (halfEdgeIdx != -1 && edges[halfEdgeIdx].isValid())
            aabb.expand(getFaceBoundingBox(halfEdgeIdx));
    }
    return aabb;
}

AABB GradMesh::getAABB() const
{
    AABB aabb{glm::vec2(std::numeric_limits<float>::max()), glm::vec2(std::numeric_limits<float>::lowest())};

    for (auto &point : points)
    {
        if (point.isValid())
            aabb.expand(point.coords);
    }
    return aabb;
}

std::vector<std::pair<int, int>> GradMesh::findCornerFace() const
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

int GradMesh::findParentPointIdx(int halfEdgeIdx) const
{
    auto *currEdge = &edges[halfEdgeIdx];
    while (currEdge->parentIdx != -1)
    {
        currEdge = &edges[currEdge->parentIdx];
    }
    return currEdge->originIdx;
}