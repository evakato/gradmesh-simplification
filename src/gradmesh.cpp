#include "gradmesh.hpp"

std::shared_ptr<std::vector<Patch>> GradMesh::generatePatchData()
{
    patches = std::make_shared<std::vector<Patch>>();

    int patchfaceIdx = 0;
    for (Face &face : faces)
    {
        std::cout << patchfaceIdx << ": ";

        const auto &topEdge = edges[face.halfEdgeIdx];
        const auto &rightEdge = edges[topEdge.nextIdx];
        const auto &bottomEdge = edges[rightEdge.nextIdx];
        const auto &leftEdge = edges[bottomEdge.nextIdx];

        if (topEdge.handleIdxs.first == -1 ||
            rightEdge.handleIdxs.first == -1 ||
            bottomEdge.handleIdxs.first == -1 ||
            leftEdge.handleIdxs.first == -1)
        {
            std::cout << "yappari sou omotta";
        }

        auto [m0, m0v, m1v, m0uv] = computeEdgeDerivatives(topEdge);
        auto [m1, m1u, m3u, m1uv] = computeEdgeDerivatives(rightEdge);
        auto [m3, m3v, m2v, m3uv] = computeEdgeDerivatives(bottomEdge);
        auto [m2, m2u, m0u, m2uv] = computeEdgeDerivatives(leftEdge);

        std::vector<Vertex> controlMatrix = {m0, m0v, m1v, m1,       //
                                             -m0u, m0uv, -m1uv, m1u, //
                                             -m2u, -m2uv, m3uv, m3u, //
                                             m2, -m2v, -m3v, m3};

        std::cout << "\n";
        patches->push_back(Patch{controlMatrix});
        patchfaceIdx++;
    }

    return patches;
}

void GradMesh::setChildrenData()
{
    for (HalfEdge &edge : edges)
    {
        if (edge.parentIdx != -1)
        {
            // likely a bandaid fix for certain .hemesh files..
            HalfEdge &parent = edges[edge.parentIdx];
            if (edge.handleIdxs.first != -1)
            {
                edge.originIdx = parent.originIdx;
                edge.handleIdxs.first = parent.handleIdxs.first;
                edge.handleIdxs.second = parent.handleIdxs.second;
            }
            else
            {
            }
        }
    }
}

CurveVector GradMesh::getCurve(int halfEdgeIdx)
{
    HalfEdge &edge = edges[halfEdgeIdx];
    auto &origin = points[edge.originIdx];
    auto &handle1 = handles[edge.handleIdxs.first];
    auto &handle2 = handles[edge.handleIdxs.second];
    auto &nextEdge = edges[edge.nextIdx];

    CurveVector curveVector{Vertex{origin.coords, edge.color}, Vertex{handle1.coords, handle1.color}, -Vertex{handle2.coords, handle2.color}, Vertex{points[nextEdge.originIdx].coords, nextEdge.color}};
    // std::cout << "parent vector";
    // std::cout << curveVector[0] << curveVector[1] << curveVector[2] << curveVector[3] << std::endl;
    return curveVector;
}

std::array<Vertex, 4> GradMesh::computeEdgeDerivatives(const HalfEdge &currEdge)
{
    std::array<Vertex, 4> edgeDerivatives;

    if (currEdge.handleIdxs.first == -1)
    {
        // std::cout << currIndex << ", " << currEdge.interval[0] << ", " << currEdge.interval[1] << "\n";

        CurveVector parentCurve = getCurve(currEdge.parentIdx);
        Vertex v = interpolateCubic(parentCurve, currEdge.interval[0]);

        edgeDerivatives[0] = Vertex{v.coords, currEdge.color};
        float scale = currEdge.interval[1] - currEdge.interval[0];
        edgeDerivatives[1] = interpolateCubicDerivative(parentCurve, currEdge.interval[0]) * scale;
        edgeDerivatives[2] = interpolateCubicDerivative(parentCurve, currEdge.interval[1]) * scale;
    }
    else
    {
        auto &origin = points[currEdge.originIdx];
        auto &handle1 = handles[currEdge.handleIdxs.first];
        auto &handle2 = handles[currEdge.handleIdxs.second];

        edgeDerivatives[0] = Vertex{origin.coords, currEdge.color};
        edgeDerivatives[1] = Vertex{handle1.coords, handle1.color};
        edgeDerivatives[2] = -Vertex{handle2.coords, handle2.color};
    }
    edgeDerivatives[3] = Vertex{currEdge.twist.coords, currEdge.twist.color};

    return edgeDerivatives;
}

std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh)
{
    for (auto point : gradMesh.points)
    {
        out << "Point with edge " << point.halfEdgeIdx << ": (" << point.coords.x << ", " << point.coords.y << ")\n";
    }
    for (int i = 0; i < gradMesh.handles.size(); i++)
    {
        auto &handle = gradMesh.handles[i];
        out << "Handle " << i << " with edge " << handle.halfEdgeIdx << ": (" << handle.coords.x << ", " << handle.coords.y << ")\n";
    }
    for (int i = 0; i < gradMesh.faces.size(); i++)
    {
        auto &patch = gradMesh.faces[i];
        out << "Face " << i << " with edge " << patch.halfEdgeIdx << "\n";
    }
    for (int i = 0; i < gradMesh.edges.size(); i++)
    {
        auto &edge = gradMesh.edges[i];
        out << "Edge " << i << " has origin: " << edge.originIdx << " and has color: (" << edge.color.x << ", " << edge.color.y << ", " << edge.color.z << "). ";
        out << "Handle idxs: " << edge.handleIdxs.first << " and " << edge.handleIdxs.second << ". ";
        out << "next is " << edge.nextIdx << " and prev is " << edge.prevIdx << "\n";
    }
    return out;
}

Vertex interpolateCubic(CurveVector curve, float t)
{
    std::cout << "t: " << t << "\n";
    glm::vec4 tVec = glm::vec4(1.0f, t, t * t, t * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}

Vertex interpolateCubicDerivative(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(0.0f, 1.0f, 2.0f * t, 3.0f * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}
