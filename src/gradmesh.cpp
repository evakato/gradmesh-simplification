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

            std::bitset<4> isChild = ((topEdge.isBar()) << 0 |
                                      (rightEdge.isBar()) << 1 |
                                      (bottomEdge.isBar()) << 2 |
                                      (leftEdge.isBar()) << 3);

            patches->push_back(Patch{controlMatrix, isChild, generatePointData()});
        }
    }

    return patches;
}

std::vector<Vertex> GradMesh::generatePointData() const
{
    std::vector<Vertex> pointData;
    for (const Point &point : points)
    {
        if (point.halfEdgeIdx != -1)
            pointData.push_back({point.coords, glm::vec3{1.0f}});
    }
    return pointData;
}
CurveVector GradMesh::getCurve(int halfEdgeIdx)
{
    HalfEdge &edge = edges[halfEdgeIdx];
    auto &origin = points[edge.originIdx];
    auto &handle1 = handles[edge.handleIdxs.first];
    auto &handle2 = handles[edge.handleIdxs.second];
    auto &nextEdge = edges[edge.nextIdx];

    CurveVector curveVector{Vertex{origin.coords, edge.color}, Vertex{handle1.coords, handle1.color}, -Vertex{handle2.coords, handle2.color}, Vertex{points[nextEdge.originIdx].coords, nextEdge.color}};
    return curveVector;
}

std::array<Vertex, 4> GradMesh::computeEdgeDerivatives(const HalfEdge &currEdge)
{
    std::array<Vertex, 4> edgeDerivatives;

    if (currEdge.parentIdx == -1)
    {
        auto &origin = points[currEdge.originIdx];
        auto &handle1 = handles[currEdge.handleIdxs.first];
        auto &handle2 = handles[currEdge.handleIdxs.second];

        edgeDerivatives[0] = Vertex{origin.coords, currEdge.color};
        edgeDerivatives[1] = Vertex{handle1.coords, handle1.color};
        edgeDerivatives[2] = -Vertex{handle2.coords, handle2.color};
    }
    else
    {
        CurveVector parentCurve = getCurve(currEdge.parentIdx);
        Vertex v = interpolateCubic(parentCurve, currEdge.interval[0]);
        edgeDerivatives[0] = Vertex{v.coords, currEdge.color};
        if (currEdge.handleIdxs.first == -1)
        {
            // std::cout << currIndex << ", " << currEdge.interval[0] << ", " << currEdge.interval[1] << "\n";
            float scale = currEdge.interval[1] - currEdge.interval[0];
            edgeDerivatives[1] = interpolateCubicDerivative(parentCurve, currEdge.interval[0]) * scale;
            edgeDerivatives[2] = interpolateCubicDerivative(parentCurve, currEdge.interval[1]) * scale;
        }
        else
        {
            auto &handle1 = handles[currEdge.handleIdxs.first];
            auto &handle2 = handles[currEdge.handleIdxs.second];
            edgeDerivatives[1] = Vertex{handle1.coords, handle1.color};
            edgeDerivatives[2] = -Vertex{handle2.coords, handle2.color};
        }
    }
    edgeDerivatives[3] = Vertex{currEdge.twist.coords, currEdge.twist.color};

    return edgeDerivatives;
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
            // likely a bandaid fix for certain .hemesh files..
            // ideally i want to get rid of all those annoying parent edges completely, but for now this will do
            HalfEdge &parent = edges[edge.parentIdx];
            std::cout << "the child is " << halfEdgeIdx << "and the parent is " << edge.parentIdx << " who belongs to " << parent.patchIdx << "\n";
            parent.childIdxDegenerate = halfEdgeIdx;
            edge.originIdx = parent.originIdx;
            edge.handleIdxs.first = parent.handleIdxs.first;
            edge.handleIdxs.second = parent.handleIdxs.second;
            edge.parentIdx = -1;
            count++;
        }
        halfEdgeIdx++;
    }
    for (Face &face : faces)
    {
        if (face.halfEdgeIdx != -1)
        {
            int currEdgeIdx = face.halfEdgeIdx;
            for (int i = 0; i < 4; i++)
            {
                auto &currEdge = edges[currEdgeIdx];
                if (currEdge.twinIdx != -1 && edges[currEdge.twinIdx].childIdxDegenerate != -1)
                    currEdge.twinIdx = edges[currEdge.twinIdx].childIdxDegenerate;
                // std::cout << "why is the twin: " << currEdge.twinIdx << std::endl;
                currEdgeIdx = currEdge.nextIdx;
            }
        }
    }
    std::cout << "There are " << count << " weird edges.\n";
}

void GradMesh::candidateMerges()
{
    int faceNum = 0;
    for (Face &face : faces)
    {
        if (face.halfEdgeIdx != -1)
        {
            std::cout << "face" << faceNum << ": ";
            int top = face.halfEdgeIdx;
            auto &topEdge = edges[top];
            int right = topEdge.nextIdx;
            auto &rightEdge = edges[right];
            int bottom = rightEdge.nextIdx;
            auto &bottomEdge = edges[bottom];
            int left = bottomEdge.nextIdx;
            std::cout << "top, right, bottom, left: " << top << ", " << right << ", " << bottom << ", " << left << "\n";
            faceNum++;
        }
    }
}

void GradMesh::replaceChildWithParent(int childEdgeIdx)
{
    auto &child = edges[childEdgeIdx];
    auto &parent = edges[child.parentIdx];
    child.originIdx = parent.originIdx;
    child.handleIdxs = parent.handleIdxs;
    child.color = parent.color;
    child.parentIdx = -1;
    child.twinIdx = parent.twinIdx;
    edges[child.twinIdx].twinIsTJunction = false;
    edges[child.twinIdx].twinIdx = childEdgeIdx;
}

void GradMesh::addTJunction(int bar1Idx, int bar2Idx, int stemIdx, int parentTwinIdx, float t)
{
    auto &bar1 = edges[bar1Idx];
    auto &bar2 = edges[bar2Idx];
    auto &stem = edges[stemIdx];
    HalfEdge parentEdge;
    parentEdge.color = bar1.color;
    std::pair<int, int> newHandles = edges[parentTwinIdx].handleIdxs;
    parentEdge.handleIdxs = {newHandles.second, newHandles.first};
    parentEdge.twinIdx = parentTwinIdx;
    parentEdge.originIdx = bar1.originIdx;
    parentEdge.nextIdx = bar2.nextIdx;
    parentEdge.parentIdx = -1;
    parentEdge.childrenIdxs = {bar1Idx, bar2Idx, stemIdx};
    int parentIdx = addEdge(parentEdge);

    points[bar2.originIdx].halfEdgeIdx = -1;

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
    bar1.twinIdx = -1;
    bar2.twinIdx = -1;
    edges[parentTwinIdx].twinIdx = parentIdx;
    edges[parentTwinIdx].twinIsTJunction = true;

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

std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh)
{
    for (int i = 0; i < gradMesh.points.size(); i++)
    {
        auto &point = gradMesh.points[i];
        out << "Point " << i << " with edge " << point.halfEdgeIdx << ": (" << point.coords.x << ", " << point.coords.y << ")\n";
    }
    for (int i = 0; i < gradMesh.handles.size(); i++)
    {
        auto &handle = gradMesh.handles[i];
        // out << "Handle " << i << " with edge " << handle.halfEdgeIdx << ": (" << handle.coords.x << ", " << handle.coords.y << ")\n";
    }
    for (int i = 0; i < gradMesh.faces.size(); i++)
    {
        auto &patch = gradMesh.faces[i];
        out << "Face " << i << " with edge " << patch.halfEdgeIdx << "\n";
    }
    size_t validEdges = 0;
    for (int i = 0; i < gradMesh.edges.size(); i++)
    {
        auto &edge = gradMesh.edges[i];
        if (edge.patchIdx == -1)
        {
            out << "Edge " << i << " is not in use\n";
        }
        else
        {
            validEdges++;
            if (edge.interval.x == edge.interval.y && edge.parentIdx != -1)
                out << "STEM ";
            out << "Edge " << i << " has origin: " << edge.originIdx << " and has color: (" << edge.color.x << ", " << edge.color.y << ", " << edge.color.z << "). ";
            out << " interval is: " << edge.interval.x << ", " << edge.interval.y << ". ";
            out << "Handle idxs: " << edge.handleIdxs.first << " and " << edge.handleIdxs.second << ". ";
            out << "next is " << edge.nextIdx << " and prev is " << edge.prevIdx;
            out << " FACE idx is " << edge.patchIdx;
            out << "parent is " << edge.parentIdx << " and twin is " << edge.twinIdx << "\n";
        }
    }
    out << "There are " << validEdges << " valid edges in the mesh\n";
    return out;
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
