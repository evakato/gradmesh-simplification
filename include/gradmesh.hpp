#pragma once

#include <iostream>
#include <vector>

#include "types.hpp"

class GradMesh
{
public:
    GradMesh() = default;

    void addPoint(float x, float y, int idx)
    {
        points.push_back(Point{glm::vec2(x, y), idx});
    }
    void addHandle(float x, float y, float r, float g, float b, int idx)
    {
        handles.push_back(Handle{glm::vec2(x, y), glm::vec3(r, g, b), idx});
    }
    void addFace(int idx)
    {
        faces.push_back(Face{idx});
    }
    void addEdge(HalfEdge edge)
    {
        edges.push_back(edge);
    }
    HalfEdge &getEdge(int idx)
    {
        return edges[idx];
    }
    std::vector<std::vector<Vertex>> getControlMatrix();
    void setChildrenData();

    friend std::ostream &operator<<(std::ostream &out, const GradMesh &gradMesh);

private:
    std::vector<Point> points;
    std::vector<Handle> handles;
    std::vector<Face> faces;
    std::vector<HalfEdge> edges;
};
