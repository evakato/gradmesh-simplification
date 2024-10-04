#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

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
    float length() const
    {
        return std::sqrt(coords.x * coords.x + coords.y * coords.y +
                         color.r * color.r + color.g * color.g + color.b * color.b);
    }
};
struct HalfEdge
{
    glm::vec2 interval;
    Vertex twist;
    glm::vec3 color;
    std::pair<int, int> handleIdxs;
    int twinIdx;
    int prevIdx;
    int nextIdx;
    int faceIdx;
    int originIdx;
    int parentIdx;
    std::vector<int> childrenIdxs = {};
    int childIdxDegenerate = -1;

    bool isChild() const
    {
        return parentIdx != -1;
    }
    bool isBar() const
    {
        // return parentIdx != -1 && (interval.x != interval.y);
        return isChild() && handleIdxs.first == -1;
    }
    bool isStem() const
    {
        // return parentIdx != -1 && (interval.x == interval.y);
        return isChild() && handleIdxs.first != -1;
    }
    bool isNormal() const
    {
        return parentIdx == -1 && childrenIdxs.size() == 0;
    }
    bool isParent() const
    {
        return childrenIdxs.size() > 0;
    }
    void copyGeometricDataExceptHandles(const HalfEdge &other)
    {
        this->originIdx = other.originIdx;
        this->color = other.color;
        this->twist = other.twist;
    }
    void copyGeometricData(const HalfEdge &other)
    {
        this->originIdx = other.originIdx;
        this->color = other.color;
        this->twist = other.twist;
        this->handleIdxs = other.handleIdxs;
    }
    void copyAll(const HalfEdge &other)
    {
        copyGeometricData(other);
        this->parentIdx = other.parentIdx;
        this->interval = other.interval;
    }
    void copyChildData(const HalfEdge &other)
    {
        this->parentIdx = other.parentIdx;
        this->interval = other.interval;
    }
    void copyParentAndHandles(const HalfEdge &other)
    {
        this->parentIdx = other.parentIdx;
        this->interval = other.interval;
        this->handleIdxs = other.handleIdxs;
    }
    void resetToNormal()
    {
        parentIdx = -1;
        interval = {0, 1};
    }
    bool hasTwin() const
    {
        return twinIdx != -1;
    }
    void replaceChild(int oldChildIdx, int newChildIdx)
    {
        for (int i = 0; i < childrenIdxs.size(); i++)
        {
            if (childrenIdxs[i] == oldChildIdx)
            {
                childrenIdxs[i] = newChildIdx;
                return;
            }
        }
    }
    void addChildrenIdxs(std::vector<int> newChildren)
    {
        for (const int &child : newChildren)
            // Check if the child is not already in childrenIdxs
            if (std::find(childrenIdxs.begin(), childrenIdxs.end(), child) == childrenIdxs.end())
                childrenIdxs.push_back(child); // Add only if not found
    }
    void disable()
    {
        faceIdx = -1;
    }
};

struct PointId
{
    int primitiveId;
    int pointId;
};

struct CurveId
{
    int patchId;
    int curveId;
};

using CurveVector = std::array<Vertex, 4>;

inline constexpr float BCM{1.0f / 3.0f}; // bezier conversion multiplier
inline constexpr int VERTS_PER_PATCH{16};
inline constexpr int VERTS_PER_CURVE{4};
inline constexpr int SCR_WIDTH{1280};
inline constexpr int SCR_HEIGHT{960};
inline constexpr int GL_LENGTH{920};
inline constexpr int GUI_WIDTH{SCR_WIDTH - GL_LENGTH};
inline constexpr int GUI_POS{SCR_WIDTH - GUI_WIDTH};
inline constexpr std::string_view IMAGE_DIR{"img"};

inline constexpr glm::vec3 blue{0.0f, 0.478f, 1.0f};
inline constexpr glm::vec3 black{0.0f};
inline constexpr glm::vec3 white{1.0f};

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
