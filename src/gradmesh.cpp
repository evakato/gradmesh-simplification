#include "gradmesh.hpp"

std::shared_ptr<std::vector<Patch>> GradMesh::generatePatchData()
{
    patches = std::make_shared<std::vector<Patch>>();

    for (Face &face : faces)
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

        patches->push_back(Patch{controlMatrix, isChild});
    }

    return patches;
}

CurveVector GradMesh::getCurve(int halfEdgeIdx) const
{
    assert(halfEdgeIdx != -1);

    const auto &edge = edges[halfEdgeIdx];

    assert(edges[halfEdgeIdx].isValid());
    assert(edge.nextIdx != -1 && edges[edge.nextIdx].isValid());

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
    std::cout << "CLEANING EDGES\n";
    int count = 0;
    int halfEdgeIdx = 0;
    for (HalfEdge &edge : edges)
    {
        if (edge.parentIdx != -1 && approximateFloating(edge.interval[0]) && edge.interval[1] == 1)
        {
            // likely a bandaid fix for certain .hemesh files.. ideally i want to get rid of all those annoying parent edges completely, but for now this will do
            HalfEdge &parent = edges[edge.parentIdx];
            std::cout << "the child is " << halfEdgeIdx << "and the parent is " << edge.parentIdx << " who belongs to " << parent.faceIdx << "\n";
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

void GradMesh::fixAndSetTwin(int barIdx)
{
    auto &bar = edges[barIdx];
    std::cout << "parent is: " << bar.parentIdx;
    auto &parent = edges[bar.parentIdx];
    auto &twin = edges[parent.twinIdx];
    if (bar.twinIdx != parent.twinIdx)
    {
        std::cout << "fixing the twin: prev: " << bar.twinIdx << " new: " << parent.twinIdx << "\n";
        bar.twinIdx = parent.twinIdx;
    }

    twin.twinIdx = barIdx;
    std::cout << "setting " << bar.twinIdx << " to " << edges[bar.twinIdx].twinIdx << ".\n";
}

// I always forget how I wrote this function, bar1 and bar2 are the literal bar1 and bar2 of the new T-junction. That is the order.
void GradMesh::addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t)
{
    assert(edge1.hasTwin() && edge2.hasTwin());

    int bar1Idx = edge1.twinIdx;
    int bar2Idx = edge2.twinIdx;
    auto &bar1 = edges[bar1Idx];
    auto &bar2 = edges[bar2Idx];
    auto twinHandles = edges[twinOfParentIdx].handleIdxs;
    int parentIdx;
    int stemIdx = bar1.nextIdx;
    if (edges[bar1.nextIdx].isBar())
    {
        stemIdx = edges[bar1.nextIdx].parentIdx;
        std::cout << "stem is bar: " << stemIdx << "\n";
    }

    if (bar2.isParent() && bar1.isParent())
    {
        // strategy: remove the parent of bar2 and update bar1
        std::cout << "Both bars are parents\n";
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        scaleUpChildrenByT(bar2, t);
        bar1.addChildrenIdxs(bar2.childrenIdxs);
        setChildrenNewParent(bar2, parentIdx);
        bar2.disable();
    }
    else if (bar1.isParent())
    {
        std::cout << "bar1 is a parent\n";
        parentIdx = bar1Idx;
        scaleDownChildrenByT(bar1, t);
        bar1.addChildrenIdxs({bar2Idx});
        bar2.createBar(parentIdx, {t, 1});
    }
    else if (bar2.isParent())
    {
        std::cout << "bar2 is a parent\n";
        parentIdx = bar2Idx;
        scaleUpChildrenByT(bar2, t);
        bar2.addChildrenIdxs({bar1Idx});
    }
    else
    {
        std::cout << "neither is a parent\n";
        parentIdx = addEdge(HalfEdge{});
        edges[parentIdx].childrenIdxs = {bar1Idx, bar2Idx};
        bar2.createBar(parentIdx, {t, 1});
    }

    if (!bar1.isParent())
    {
        if (bar1.isStem())
        {
            // 11 --> 40 -- > 23
            std::cout << "Extending stem!\n";
            copyAndReplaceChild(parentIdx, bar1Idx);
        }
        edges[parentIdx].copyGeometricDataExceptHandles(bar1);
        bar1.createBar(parentIdx, {0, t});
    }

    edges[stemIdx].createStem(parentIdx, t);
    auto &parent = edges[parentIdx];
    parent.handleIdxs = {twinHandles.second, twinHandles.first};
    parent.twinIdx = twinOfParentIdx;
    parent.nextIdx = bar2.nextIdx;
    parent.addChildrenIdxs({stemIdx});
    setBarChildrensTwin(parent, twinOfParentIdx);
    edges[twinOfParentIdx].twinIdx = parentIdx;
}

void GradMesh::removeFace(int faceIdx)
{
    auto &face = faces[faceIdx];
    auto &e1 = edges[face.halfEdgeIdx];
    auto &e2 = edges[e1.nextIdx];
    auto &e3 = edges[e2.nextIdx];
    auto &e4 = edges[e3.nextIdx];
    face.halfEdgeIdx = e1.faceIdx = e2.faceIdx = e3.faceIdx = e4.faceIdx = -1;
}

void GradMesh::copyEdgeTwin(int e1Idx, int e2Idx)
{
    auto &e1 = edges[e1Idx];
    auto &e2 = edges[e2Idx];
    e1.twinIdx = e2.twinIdx;
    // don't set the twin if edge2 is a bar b/c it should point to the parent
    if (e2.hasTwin() && !e2.isBar())
    {
        auto &twin = edges[e2.twinIdx];
        if (twin.isParent())
        {
            for (int childIdx : twin.childrenIdxs)
            {
                if (edges[childIdx].isBar())
                    edges[childIdx].twinIdx = e1Idx;
            }
        }
        twin.twinIdx = e1Idx;
    }
}

bool GradMesh::twinFaceIsCycle(const HalfEdge &e) const
{
    if (e.twinIdx == -1)
        return false;

    auto &e1 = edges[e.twinIdx];
    int count = 0;

    if (e1.isChild() || e1.isParent())
        ++count;
    auto &e2 = edges[e1.nextIdx];
    if (e2.isChild() || e2.isParent())
        ++count;
    auto &e3 = edges[e2.nextIdx];
    if (e3.isChild() || e3.isParent())
        ++count;
    auto &e4 = edges[e3.nextIdx];
    if (e4.isChild() || e4.isParent())
        ++count;

    return count >= 3;
}

void GradMesh::leftTUpdateInterval(int parentIdx, float totalCurve)
{
    auto &parentEdge = edges[parentIdx];
    for (int childIdx : parentEdge.childrenIdxs)
    {
        auto &child = edges[childIdx];
        if (child.parentIdx != parentIdx)
            continue; // strong check b/c i wrote some bad code

        if (child.isRightMostChild())
            child.interval.x /= totalCurve;
        else
            child.interval /= totalCurve;
    }
}

void GradMesh::scaleDownChildrenByT(HalfEdge &parentEdge, float t)
{
    for (int childIdx : parentEdge.childrenIdxs)
        edges[childIdx].interval *= t;
}
void GradMesh::scaleUpChildrenByT(HalfEdge &parentEdge, float t)
{
    for (int childIdx : parentEdge.childrenIdxs)
    {
        edges[childIdx].interval *= (1 - t);
        edges[childIdx].interval += t;
    }
}

void GradMesh::setBarChildrensTwin(HalfEdge &parentEdge, int twinIdx)
{
    for (int childIdx : parentEdge.childrenIdxs)
        if (edges[childIdx].isBar())
            edges[childIdx].twinIdx = twinIdx;
}

void GradMesh::setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx)
{
    for (int childIdx : parentEdge.childrenIdxs)
        edges[childIdx].parentIdx = newParentIdx;
}