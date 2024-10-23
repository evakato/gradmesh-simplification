#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

struct AABB
{
    glm::vec2 min;
    glm::vec2 max;

    void expand(const AABB &other)
    {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
    void expand(const glm::vec2 &other)
    {
        min = glm::min(min, other);
        max = glm::max(max, other);
    }
    glm::vec2 getPixelDimensions(const int maxLength)
    {
        glm::vec2 diff = max - min;
        if (diff.x > diff.y)
            return glm::vec2{maxLength, (diff.y / diff.x) * maxLength};

        return glm::vec2{(diff.x / diff.y) * maxLength, maxLength};
    }
    void addPadding(float padding)
    {
        min -= padding;
        max += padding;
    }
};

struct Point
{
    glm::vec2 coords;
    int halfEdgeIdx;
    bool isValid() const { return halfEdgeIdx != -1; }
    void disable() { halfEdgeIdx = -1; }
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
    bool isValid() const { return halfEdgeIdx != -1; }
};
struct Face
{
    int halfEdgeIdx;
    bool isValid() const { return halfEdgeIdx != -1; }
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
    glm::vec2 interval = {0, 1};
    Vertex twist;
    glm::vec3 color;
    std::pair<int, int> handleIdxs;
    int twinIdx;
    int prevIdx;
    int nextIdx;
    int faceIdx;
    int originIdx;
    int parentIdx = -1;
    std::vector<int> childrenIdxs = {};
    int childIdxDegenerate = -1;

    bool isValid() const
    {
        return faceIdx != -1;
    }
    bool isChild() const
    {
        return parentIdx != -1;
    }
    bool isBar() const
    {
        return isChild() && handleIdxs.first == -1;
    }
    bool isStem() const
    {
        return isChild() && handleIdxs.first != -1;
    }
    bool isParent() const
    {
        return childrenIdxs.size() > 0;
    }
    bool isStemParent() const
    {
        return isStem() && isParent();
    }
    bool hasTwin() const
    {
        return twinIdx != -1;
    }
    bool isLeftMostChild() const
    {
        return isBar() && interval.x == 0.0f;
    }
    bool isRightMostChild() const
    {
        return isBar() && interval.y == 1.0f;
    }
    void disable()
    {
        faceIdx = -1;
    }
    void copyGeometricData(const HalfEdge &other)
    {
        this->originIdx = other.originIdx;
        this->color = other.color;
        this->twist = other.twist;
    }
    void copyChildData(const HalfEdge &other)
    {
        this->parentIdx = other.parentIdx;
        this->interval = other.interval;
    }
    void replaceChild(int oldChildIdx, int newChildIdx)
    {
        std::replace(childrenIdxs.begin(), childrenIdxs.end(), oldChildIdx, newChildIdx);
        // auto endIt = std::unique(childrenIdxs.begin(), childrenIdxs.end());
        // childrenIdxs.erase(endIt, childrenIdxs.end());
    }
    void addChildrenIdxs(std::vector<int> newChildren)
    {
        for (const int &child : newChildren)
            // Check if the child is not already in childrenIdxs
            if (std::find(childrenIdxs.begin(), childrenIdxs.end(), child) == childrenIdxs.end())
                childrenIdxs.push_back(child); // Add only if not found
    }
    void createStem(int newParentIdx, glm::vec2 newInterval)
    {
        originIdx = -1;
        parentIdx = newParentIdx;
        interval = newInterval;
    }
    void createBar(int newParentIdx, glm::vec2 newInterval)
    {
        createStem(newParentIdx, newInterval);
        handleIdxs = {-1, -1};
    }
    void removeChildIdx(int childIdx)
    {
        auto it = std::remove(childrenIdxs.begin(), childrenIdxs.end(), childIdx);
        childrenIdxs.erase(it, childrenIdxs.end());
    }
};

inline std::vector<int> getValidCompIndices(const auto &comps)
{
    std::vector<int> indices;
    for (int i = 0; i < comps.size(); ++i)
        if (comps[i].isValid())
            indices.push_back(i);
    return indices;
}

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
inline constexpr int POOLING_LENGTH{200};
inline constexpr float AABB_PADDING{0.05f};
inline constexpr std::string_view IMAGE_DIR{"img"};
inline constexpr std::string_view LOGS_DIR{"logs"};
inline constexpr std::string_view SAVES_DIR{"mesh_saves"};
inline const char *MERGE_METRIC_IMG{"img/mergedMesh.png"};
inline const char *ORIG_METRIC_IMG{"img/origMesh.png"};
inline constexpr float ERROR_THRESHOLD{0.001f};
inline constexpr int MAX_CURVE_DEPTH = 1000;
inline constexpr int GUI_IMAGE_SIZE{150};

inline constexpr glm::vec3 blue{0.0f, 0.478f, 1.0f};
inline constexpr glm::vec3 black{0.0f};
inline constexpr glm::vec3 white{1.0f};
