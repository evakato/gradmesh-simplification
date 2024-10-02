#include "gradmesh.hpp"

std::shared_ptr<std::vector<Patch>> GradMesh::generatePatchData()
{
    patches = std::make_shared<std::vector<Patch>>();

    for (Face &face : faces)
    {
        if (face.halfEdgeIdx != -1)
        {
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

            std::bitset<4> isBar = ((topEdge.isBar()) << 0 |
                                    (rightEdge.isBar()) << 1 |
                                    (bottomEdge.isBar()) << 2 |
                                    (leftEdge.isBar()) << 3);

            std::bitset<4> isChild = ((topEdge.isChild()) << 0 |
                                      (rightEdge.isChild()) << 1 |
                                      (bottomEdge.isChild()) << 2 |
                                      (leftEdge.isChild()) << 3);

            patches->push_back(Patch{controlMatrix, isChild});
        }
    }

    return patches;
}

CurveVector GradMesh::getCurve(int halfEdgeIdx) const
{
    const auto &edge = edges[halfEdgeIdx];
    auto [m0, m0v, m1v, m0uv] = computeEdgeDerivatives(edge);
    auto m1 = computeEdgeDerivatives(edges[edge.nextIdx])[0];
    return CurveVector{m0, m0v, m1v, m1};
}

std::array<Vertex, 4> GradMesh::computeEdgeDerivatives(const HalfEdge &edge) const
{
    std::array<Vertex, 4> edgeDerivatives;

    if (!edge.isChild())
    {
        edgeDerivatives[0] = Vertex{points[edge.originIdx].coords, edge.color};
        edgeDerivatives[1] = Vertex{handles[edge.handleIdxs.first]};
        edgeDerivatives[2] = -Vertex{handles[edge.handleIdxs.second]};
    }
    else
    {
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
            edgeDerivatives[1] = Vertex{handles[edge.handleIdxs.first]};
            edgeDerivatives[2] = -Vertex{handles[edge.handleIdxs.second]};
        }
    }
    edgeDerivatives[3] = edge.twist;

    return edgeDerivatives;
}

std::vector<Vertex> GradMesh::getHandles() const
{
    std::vector<Vertex> pointsAndHandles = {};
    for (auto &edge : edges)
    {
        if (edge.patchIdx != -1 && !edge.isChild())
        {
            assert(edge.originIdx != -1);
            assert(edge.handleIdxs.first != -1);
            auto &point = points[edge.originIdx];
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
    std::cout << "CLEANING EDGES\n";
    int count = 0;
    int halfEdgeIdx = 0;
    for (HalfEdge &edge : edges)
    {
        if (edge.parentIdx != -1 && approximateFloating(edge.interval[0]) && edge.interval[1] == 1)
        {
            // likely a bandaid fix for certain .hemesh files.. ideally i want to get rid of all those annoying parent edges completely, but for now this will do
            HalfEdge &parent = edges[edge.parentIdx];
            std::cout << "the child is " << halfEdgeIdx << "and the parent is " << edge.parentIdx << " who belongs to " << parent.patchIdx << "\n";
            parent.childIdxDegenerate = halfEdgeIdx;
            parent.patchIdx = -1;
            edge.originIdx = parent.originIdx;
            edge.handleIdxs = parent.handleIdxs;
            edge.parentIdx = -1;
            count++;
        }
        halfEdgeIdx++;
    }
    for (Face &face : faces)
    {
        if (face.halfEdgeIdx != -1)
        {
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
    }
    std::cout << "There are " << count << " weird edges.\n";
}

void GradMesh::replaceChildWithParent(int childEdgeIdx)
{
    auto &child = edges[childEdgeIdx];
    child.copyAll(edges[child.parentIdx]);
    copyEdgeTwin(childEdgeIdx, child.parentIdx);
}

void GradMesh::addTJunction(int bar1Idx, int bar2Idx, int stemIdx, int parentTwinIdx, float t, bool extendStem)
{
    auto &bar1 = edges[bar1Idx];
    auto &bar2 = edges[bar2Idx];
    auto &stem = edges[stemIdx];
    HalfEdge parentEdge;
    if (extendStem)
    {
        std::cout << "Extending stem!\n";
        parentEdge.copyAll(bar1);
    }
    else
    {
        parentEdge.copyGeometricData(bar1);
        parentEdge.parentIdx = -1;
        parentEdge.interval = {0, 1};
    }
    std::pair<int, int> newHandles = edges[parentTwinIdx].handleIdxs;
    parentEdge.handleIdxs = {newHandles.second, newHandles.first};
    parentEdge.twinIdx = parentTwinIdx;
    parentEdge.nextIdx = bar2.nextIdx;
    parentEdge.childrenIdxs = {bar1Idx, bar2Idx, stemIdx};
    int parentIdx = addEdge(parentEdge);
    // points[bar2.originIdx].halfEdgeIdx = -1;

    bar1.parentIdx = parentIdx;
    bar2.parentIdx = parentIdx;
    stem.parentIdx = parentIdx;
    bar1.handleIdxs = {-1, -1};
    bar2.handleIdxs = {-1, -1};
    bar1.originIdx = -1;
    bar2.originIdx = -1;
    stem.originIdx = -1;
    bar1.interval = {0, t};
    bar2.interval = {t, 1};
    stem.interval = {t, t};
    bar1.twinIdx = parentTwinIdx;
    bar2.twinIdx = parentTwinIdx;
    edges[parentTwinIdx].twinIdx = parentIdx;

    // TODO: this needs to be expanded recursively
    std::cout << parentEdge.childrenIdxs[0] << std::endl;

    for (int childIdx : bar1.childrenIdxs)
    {
        edges[childIdx].parentIdx = parentIdx;
        edges[childIdx].interval *= t;
        // std::cout << "child: " << childIdx << " interval: " << edges[childIdx].interval << " parent: " << edges[childIdx].parentIdx << std::endl;
    }
    for (int childIdx : bar2.childrenIdxs)
    {
        std::cout << "inside bar 2 child" << std::endl;
        edges[childIdx].parentIdx = parentIdx;
        edges[childIdx].interval *= t;
    }
}

Vertex interpolateCubic(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(1.0f, t, t * t, t * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}

Vertex interpolateCubicDerivative(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(0.0f, 1.0f, 2.0f * t, 3.0f * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}
