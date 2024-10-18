#include "gradmesh.hpp"
#include "ostream_ops.hpp"

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
        int top = patch.halfEdgeIdx;
        if (top != -1)
        {
            out << "Face " << i << ": ";
            auto &topEdge = gradMesh.edges[top];
            int right = topEdge.nextIdx;
            auto &rightEdge = gradMesh.edges[right];
            int bottom = rightEdge.nextIdx;
            auto &bottomEdge = gradMesh.edges[bottom];
            int left = bottomEdge.nextIdx;
            out << "top, right, bottom, left: " << top << ", " << right << ", " << bottom << ", " << left << "\n";
        }
    }
    size_t validEdges = 0;
    for (int i = 0; i < gradMesh.edges.size(); i++)
    {
        auto &edge = gradMesh.edges[i];
        if (edge.faceIdx == -1)
        {
            // out << "Edge " << i << " is not in use\n";
        }
        else
        {
            validEdges++;
            out << "Edge " << i;
            if (edge.isStem())
                out << " is stem, ";
            else if (edge.isBar())
                out << " is bar, ";
            if (edge.isParent())
                out << " is parent";
            out << edge;
        }
    }
    out << "There are " << validEdges << " valid edges in the mesh\n";
    return out;
}

std::ostream &operator<<(std::ostream &os, const Face &face)
{
    os << "  Half-edge Index: " << face.halfEdgeIdx;
    return os;
}
std::ostream &operator<<(std::ostream &os, const Point &point)
{
    os << "  Coords: (" << point.coords.x << ", " << point.coords.y << ")\n";
    os << "  Half-edge Index: " << point.halfEdgeIdx;
    return os;
}
std::ostream &operator<<(std::ostream &os, const Handle &handle)
{
    os << "  Coords: (" << std::fixed << std::setprecision(2) << handle.coords.x << ", " << handle.coords.y << ")\n";
    os << "  Color: (" << handle.color.x << ", " << handle.color.y << ", " << handle.color.z << ")\n";
    os << "  Half-edge Index: " << handle.halfEdgeIdx;
    return os;
}
std::ostream &operator<<(std::ostream &os, const HalfEdge &edge)
{
    os << std::fixed << std::setprecision(3);
    if (edge.isParent() && edge.isStem())
        os << "  Parent and Stem";
    else if (edge.isParent())
        os << "  Parent";
    else if (edge.isStem())
        os << "  Stem";
    else if (edge.isBar())
        os << "  Bar";
    else
        os << "  Interval";

    os << ": [" << edge.interval.x << ", " << edge.interval.y << "]";

    os
        << "\n  Color: (" << std::fixed << std::setprecision(2) << edge.color.x << ", " << edge.color.y << ", " << edge.color.z << ")"
        << "\n  Twist:"
        << "\n\t Coords: (" << edge.twist.coords.x << ", " << edge.twist.coords.y << ")"
        << "\n\t Color: (" << edge.twist.color.x << ", " << edge.twist.color.y << ", " << edge.twist.color.z << "))"
        << "\n  Handle Indices: (" << edge.handleIdxs.first << ", " << edge.handleIdxs.second << ")"
        << "\n  Origin Index: " << edge.originIdx
        << "\n  Patch Index: " << edge.faceIdx
        << "\n  Prev Index: " << edge.prevIdx
        << "\n  Next Index: " << edge.nextIdx
        << "\n  Twin Index: " << edge.twinIdx
        << "\n  Parent Index: " << edge.parentIdx
        << "\n  Child Index Degenerate: " << edge.childIdxDegenerate
        << "\n  Children Indices: \n  [";

    // Print children indices
    for (size_t i = 0; i < edge.childrenIdxs.size(); ++i)
    {
        os << edge.childrenIdxs[i];
        if (i != edge.childrenIdxs.size() - 1)
        {
            os << ", ";
        }
        if (i != 0 && i % 6 == 0 && i != edge.childrenIdxs.size() - 1)
            os << "\n  ";
    }
    os << "]\n";

    return os;
}