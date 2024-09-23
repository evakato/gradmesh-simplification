#pragma once

#include <cmath>
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
    Handle &operator*=(float scalar)
    {
        coords *= scalar;
        color *= scalar;
        return *this;
    }
    Handle operator*(float scalar) const
    {
        return Handle{
            coords * scalar,
            color * scalar,
            halfEdgeIdx};
    }
};
struct Face
{
    int halfEdgeIdx;
};
struct HalfEdge
{
    struct Twist
    {
        glm::vec2 coords;
        glm::vec3 color;
    };
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
    int childIdxDegenerate = -1;
    bool twinIsTJunction = false;
    std::vector<int> childrenIdxs = {};
    bool isBar() const
    {
        return parentIdx != -1 && (interval.x != interval.y);
    }
    bool isStem() const
    {
        return parentIdx != -1 && (interval.x == interval.y);
    }
};

struct Vertex
{
    glm::vec2 coords;
    glm::vec3 color;
    Vertex(const glm::vec2 &coords_init = glm::vec2(0.0f),
           const glm::vec3 &color_init = glm::vec3(1.0f))
        : coords(coords_init), color(color_init)
    {
    }
    Vertex(const Handle &handle)
        : coords(handle.coords), color(handle.color)
    {
    }
    Vertex(const glm::vec2 coords) : coords(coords)
    {
        color = glm::vec3(1.0f);
    }
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

// bezier conversion multiplier
inline constexpr float BCM{1.0f / 3.0f};
inline constexpr int VERTS_PER_PATCH{16};
inline constexpr int VERTS_PER_CURVE{4};
inline constexpr int SCR_WIDTH{1280};
inline constexpr int SCR_HEIGHT{960};
inline constexpr int GL_LENGTH{920};
inline constexpr int GUI_WIDTH{SCR_WIDTH - GL_LENGTH};
inline constexpr int GUI_POS{SCR_WIDTH - GUI_WIDTH};
inline constexpr std::string_view IMAGE_DIR{"img"};

using CurveVector = std::array<Vertex, 4>;

inline std::ostream &operator<<(std::ostream &os, const glm::vec2 &coords)
{
    os << "Coords: (" << coords.x << ", " << coords.y << "), ";
    return os;
}
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

inline bool approximateFloating(float value, float eps = 1e-5)
{
    return std::abs(value) <= eps;
}
