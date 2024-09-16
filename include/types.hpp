#pragma once

#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

struct PointId
{
    int primitiveId;
    int pointId;
};

struct Point
{
    glm::vec2 coords;
    int halfEdgeIdx;
};
struct Handle
{
    glm::vec2 coords;
    glm::vec3 color;
    int halfEdgeIdx;
};
struct Face
{
    int halfEdgeIdx;
};
struct Twist
{
    glm::vec2 coords;
    glm::vec3 color;
};
struct HalfEdge
{
    glm::vec2 interval;
    Twist twist;
    glm::vec3 color;
    std::pair<int, int> handleIdxs;
    int twinIdx;
    int prevIdx;
    int nextIdx;
    int patchIdx;
    int originIdx;
    int parentIdx;
};

struct Vertex
{
    glm::vec2 coords;
    glm::vec3 color;
    Vertex operator*(float scalar) const
    {
        return Vertex{
            coords * scalar,
            color * scalar};
    }
    Vertex operator+(const Vertex &other) const
    {
        return Vertex{
            coords + other.coords,
            color + other.color};
    }
    Vertex operator-() const
    {
        return Vertex{-coords, -color};
    }
};

inline constexpr int VERTS_PER_PATCH{16};
inline constexpr int VERTS_PER_CURVE{4};
inline constexpr int SCR_WIDTH{1280};
inline constexpr int SCR_HEIGHT{960};
inline constexpr int GL_LENGTH{920};
inline constexpr int GUI_WIDTH{SCR_WIDTH - GL_LENGTH};
inline constexpr int GUI_POS{SCR_WIDTH - GUI_WIDTH};
inline constexpr std::string_view IMAGE_DIR{"img"};

inline constexpr unsigned int curveIndicesForPatch[] = {
    0, 4, 5, 1,
    3, 11, 10, 2,
    0, 6, 8, 2,
    3, 9, 7, 1};

inline constexpr unsigned int handleIndicesForPatch[] = {
    0, 4,
    0, 6,
    1, 5,
    1, 7,
    2, 8,
    2, 10,
    3, 9,
    3, 11};

inline constexpr unsigned int cornerIndices[4] = {0, 3, 12, 15};

inline std::vector<unsigned int>
generateCurveEBO(int numPatches)
{
    std::vector<unsigned int> indices;
    for (int set = 0; set < numPatches; ++set)
    {
        int baseIndex = set * 12;
        for (int i = 0; i < 16; ++i)
            indices.push_back(curveIndicesForPatch[i] + baseIndex);
    }
    return indices;
}
inline std::vector<unsigned int> generateHandleEBO(int numVertices)
{
    std::vector<unsigned int> indices;
    for (int i = 0; i < numVertices; i += 3)
    {
        indices.push_back(i);     // First line's start
        indices.push_back(i + 1); // First line's end
        indices.push_back(i);     // Second line's start
        indices.push_back(i + 2); // Second line's end
    }
    return indices;
}

using CurveVector = std::array<Vertex, 4>;

inline std::ostream &operator<<(std::ostream &os, const Vertex &vertex)
{
    os << "Coords: (" << vertex.coords.x << ", " << vertex.coords.y << "), "
       << "Color: (" << vertex.color.r << ", " << vertex.color.g << ", " << vertex.color.b << ")";
    return os;
}

// glm is column-major
constexpr glm::mat4 hermiteBasisMat = glm::mat4(
    1.0f, 0.0f, -3.0f, 2.0f, // First column
    0.0f, 1.0f, -2.0f, 1.0f, // Second column
    0.0f, 0.0f, -1.0f, 1.0f, // Third column
    0.0f, 0.0f, 3.0f, -2.0f  // Fourth column
);