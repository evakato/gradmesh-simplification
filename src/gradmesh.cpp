#include "gradmesh.hpp"

std::vector<Patch> GradMesh::generatePatchData() const
{
    std::vector<Patch> patches{};

    for (const auto &face : faces)
    {
        if (!face.isValid())
            continue;

        const auto &topEdge = edges[face.halfEdgeIdx];
        const auto &rightEdge = edges[topEdge.nextIdx];
        const auto &bottomEdge = edges[rightEdge.nextIdx];
        const auto &leftEdge = edges[bottomEdge.nextIdx];

        auto [m0, m0v, m1v, m0uv] = computeEdgeDerivatives(topEdge);
        auto [m1, m1u, m3u, m1uv] = computeEdgeDerivatives(rightEdge);
        auto [m3, m3v, m2v, m3uv] = computeEdgeDerivatives(bottomEdge);
        auto [m2, m2u, m0u, m2uv] = computeEdgeDerivatives(leftEdge);

        std::vector<Vertex> controlMatrix = {m0, m0v, m1v, m1,       //
                                             -m0u, m0uv, -m1uv, m1u, //
                                             -m2u, -m2uv, m3uv, m3u, //
                                             m2, -m2v, -m3v, m3};

        std::bitset<4> isChild = ((topEdge.isChild()) << 0 |
                                  (rightEdge.isChild()) << 1 |
                                  (bottomEdge.isChild()) << 2 |
                                  (leftEdge.isChild()) << 3);

        patches.push_back(Patch{controlMatrix, isChild});
    }

    return patches;
}

CurveVector GradMesh::getCurve(int halfEdgeIdx) const
{
    assert(halfEdgeIdx != -1);

    const auto &edge = edges[halfEdgeIdx];

    if (!edges[halfEdgeIdx].isValid())
    {
        std::cerr << "half edge idx not valid: " << halfEdgeIdx << std::endl;
        assert(false);
    }
    // assert(edges[edge.nextIdx].isValid());

    // std::cout << "recursion: " << halfEdgeIdx << "\n";
    auto [m0, m0v, m1v, m0uv] = computeEdgeDerivatives(edge);
    auto m1 = computeEdgeDerivatives(edges[edge.nextIdx])[0];
    return CurveVector{m0, m0v, m1v, m1};
}

std::array<Vertex, 4> GradMesh::computeEdgeDerivatives(const HalfEdge &edge) const
{
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

        CurveVector parentCurve = getCurve(edge.parentIdx);
        Vertex v = interpolateCubic(parentCurve, edge.interval[0]);
        edgeDerivatives[0] = Vertex{v.coords, edge.color};
        if (edge.isBar())
        {
            float scale = edge.interval[1] - edge.interval[0];
            edgeDerivatives[1] = interpolateCubicDerivative(parentCurve, edge.interval[0]) * scale;
            edgeDerivatives[2] = interpolateCubicDerivative(parentCurve, edge.interval[1]) * scale;
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

            auto &point = computeEdgeDerivatives(edge)[0];
            auto &tangent = handles[edge.handleIdxs.first];
            pointsAndHandles.push_back(Vertex{point.coords, black});
            pointsAndHandles.push_back(Vertex{hermiteToBezier(point.coords, BCM, tangent.coords), black});
            auto &m1 = computeEdgeDerivatives(edges[edge.nextIdx])[0];
            auto &m1v = handles[edge.handleIdxs.second];
            pointsAndHandles.push_back(Vertex{m1.coords, black});
            pointsAndHandles.push_back(Vertex{hermiteToBezier(m1.coords, BCM, m1v.coords), black});
        }
    }
    return pointsAndHandles;
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
    std::cout << "There are " << count << " weird edges.\n";
}

std::vector<int> GradMesh::getBarChildren(const HalfEdge &parent) const
{
    std::vector<int> barChildrenIdxs;
    std::ranges::copy_if(
        parent.childrenIdxs,
        std::back_inserter(barChildrenIdxs),
        [this](int childIdx)
        { return edges[childIdx].isBar(); });
    return barChildrenIdxs;
}

std::vector<int> GradMesh::getStemParentChildren(const HalfEdge &parent) const
{
    std::vector<int> stemParentChildren;
    std::ranges::copy_if(
        parent.childrenIdxs,
        std::back_inserter(stemParentChildren),
        [this](int childIdx)
        { return edges[childIdx].isStemParent(); });
    return stemParentChildren;
}

bool GradMesh::incidentFaceCycle(int edgeIdx) const
{
    if (edgeIdx == -1)
        return false;

    int currIdx = edgeIdx;
    int childCount = 0;
    for (int i = 0; i < 4; i++)
    {
        auto &currEdge = edges[currIdx];
        if (currEdge.isRightMostChild())
            childCount++;
        currIdx = currEdge.nextIdx;
    }
    return childCount >= 3;
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