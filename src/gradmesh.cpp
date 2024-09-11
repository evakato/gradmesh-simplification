#include "gradmesh.hpp"

std::vector<std::vector<Vertex>> GradMesh::getControlMatrix()
{
    std::vector<std::vector<Vertex>> controlMatrices;
    for (Face &face : faces)
    {
        std::vector<Vertex> controlMatrix(16);
        auto &currEdge = edges[face.halfEdgeIdx];
        for (int i = 0; i < 4; i++)
        {
            auto &origin = points[currEdge.originIdx];
            auto &handle1 = handles[currEdge.handleIdxs.first];
            auto &handle2 = handles[currEdge.handleIdxs.second];
            controlMatrix[cmi[i][0]] = Vertex{origin.coords, currEdge.color};
            controlMatrix[cmi[i][1]] = Vertex{(tw[i][0] * handle1.coords), handle1.color};
            controlMatrix[cmi[i][2]] = Vertex{(tw[i][1] * handle2.coords), handle2.color};
            controlMatrix[cmi[i][3]] = Vertex{currEdge.twist.coords, currEdge.twist.color};
            currEdge = edges[currEdge.nextIdx];
        }
        controlMatrices.push_back(controlMatrix);
        for (int i = 0; i < 16; i++)
        {
            // std::cout << i << ": " << controlMatrix[i].coords.x << ", " << controlMatrix[i].coords.y << "\n";
        }
    }
    return controlMatrices;
}

void GradMesh::setChildrenData()
{
    for (HalfEdge &edge : edges)
    {
        if (edge.parentIdx != -1)
        {
            // likely a bandaid fix for certain .hemesh files..
            HalfEdge &parent = edges[edge.parentIdx];
            edge.originIdx = parent.originIdx;
            edge.handleIdxs.first = parent.handleIdxs.first;
            edge.handleIdxs.second = parent.handleIdxs.second;
        }
    }
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