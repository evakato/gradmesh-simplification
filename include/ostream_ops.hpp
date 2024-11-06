#pragma once

#include <iostream>
#include <iomanip>

#include <glm/glm.hpp>

#include "types.hpp"

std::ostream &operator<<(std::ostream &os, const std::vector<int> &vec);
std::ostream &operator<<(std::ostream &os, const HalfEdge &edge);
std::ostream &operator<<(std::ostream &os, const Face &face);
std::ostream &operator<<(std::ostream &os, const Point &point);
std::ostream &operator<<(std::ostream &os, const Handle &handle);
inline std::ostream &operator<<(std::ostream &os, const CurveId &id)
{
    os << "patch id: " << id.patchId << ", curve id: " << id.curveId;
    return os;
}
inline std::ostream &operator<<(std::ostream &os, const glm::vec2 &coords)
{
    os << "vec2(" << coords.x << ", " << coords.y << ")\n";
    return os;
}
inline std::ostream &operator<<(std::ostream &os, const Vertex &vertex)
{
    os << "coords: (" << vertex.coords.x << ", " << vertex.coords.y << "), "
       << "col: (" << vertex.color.r << ", " << vertex.color.g << ", " << vertex.color.b << ")";
    return os;
}
inline std::ostream &operator<<(std::ostream &os, const AABB &aabb)
{
    os << "AABB(min: (" << aabb.min.x << ", " << aabb.min.y << "), "
       << "max: (" << aabb.max.x << ", " << aabb.max.y << "))";
    return os;
}
