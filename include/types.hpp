#pragma once

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

struct PointId
{
    int primitiveId;
    int pointId;
};

struct CurveId
{
    int patchId;
    int curveId;
    bool isNull() const { return patchId == -1 || curveId == -1; }
};

inline bool operator==(const CurveId &lhs, const CurveId &rhs)
{
    return lhs.patchId == rhs.patchId && lhs.curveId == rhs.curveId;
}

struct DoubleHalfEdge
{
    int halfEdgeIdx1;
    int halfEdgeIdx2;
    CurveId curveId1;
    CurveId curveId2;
    float error;
    DoubleHalfEdge() : halfEdgeIdx1(-1), halfEdgeIdx2(-1), curveId1(CurveId()) {}
    DoubleHalfEdge(int heIdx1, int heIdx2, CurveId curveId1) : halfEdgeIdx1(heIdx1), halfEdgeIdx2(heIdx2), curveId1(curveId1) {};

    int getHalfEdgeIdx() const
    {
        return halfEdgeIdx1;
    }
    int getHalfEdgeIdx2() const
    {
        return halfEdgeIdx2;
    }
    bool matches(CurveId other)
    {
        return curveId1 == other || curveId2 == other;
    }
};

inline int findDoubleHalfEdgeIndex(const std::vector<DoubleHalfEdge> &edges, int halfEdgeIdx)
{
    auto it = std::find_if(edges.begin(), edges.end(), [halfEdgeIdx](const DoubleHalfEdge &dhe)
                           { return dhe.halfEdgeIdx1 == halfEdgeIdx || dhe.halfEdgeIdx2 == halfEdgeIdx; });

    if (it != edges.end())
    {
        return static_cast<int>(std::distance(edges.begin(), it));
    }
    return -1;
}

struct AABB
{
    glm::vec2 min;
    glm::vec2 max;

    AABB()
    {
        min = glm::vec2(std::numeric_limits<float>::max());
        max = glm::vec2(std::numeric_limits<float>::lowest());
    }
    AABB(glm::vec2 min, glm::vec2 max) : min{min}, max{max} {}
    float width() const { return max.x - min.x; }
    float length() const { return max.y - min.y; }
    float area() const { return width() * length(); }

    std::pair<int, int> getRes(int res) const
    {
        if (width() > length())
            return {res, length() / width() * res};
        return {width() / length() * res, res};
    }

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
    void constrain(const AABB &other)
    {
        min = glm::max(min, other.min);
        max = glm::min(max, other.max);
    }
    bool contains(const glm::vec2 &point) const
    {
        return (point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y);
    }
    void addPadding(float padding)
    {
        min -= padding;
        max += padding;
    }
    void ensureSize(const glm::vec2 &targetSize)
    {
        glm::vec2 currentSize = max - min;
        if (currentSize.x > targetSize.x && currentSize.y > targetSize.y)
            return;

        glm::vec2 newMin = min;
        glm::vec2 newMax = min + targetSize;
        if (currentSize.x < targetSize.x)
        {
            float centerX = (min.x + max.x) / 2.0f;
            newMin.x = centerX - (targetSize.x / 2.0f);
            newMax.x = centerX + (targetSize.x / 2.0f);
        }
        if (currentSize.y < targetSize.y)
        {
            float centerY = (min.y + max.y) / 2.0f;
            newMin.y = centerY - (targetSize.y / 2.0f);
            newMax.y = centerY + (targetSize.y / 2.0f);
        }

        min = newMin;
        max = newMax;
    }
    void resizeToSquare()
    {
        glm::vec2 currentSize = max - min;
        float maxLength = glm::max(currentSize.x, currentSize.y);
        float centerX = (min.x + max.x) / 2.0f;
        float centerY = (min.y + max.y) / 2.0f;
        min.x = centerX - (maxLength / 2.0f);
        max.x = centerX + (maxLength / 2.0f);
        min.y = centerY - (maxLength / 2.0f);
        max.y = centerY + (maxLength / 2.0f);
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

inline std::string extractFileName(const std::string &filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t lastDot = filepath.find_last_of('.');
    if (lastDot == std::string::npos || lastDot < start)
    {
        return filepath.substr(start);
    }
    return filepath.substr(start, lastDot - start);
}

template <typename T>
inline double calculateAverage(const std::vector<T> &vec)
{
    if (vec.empty())
        return T(0); // Avoid division by zero, return default value for the type

    T sum = std::accumulate(vec.begin(), vec.end(), T(0));
    return sum / static_cast<double>(vec.size());
}

inline void printElapsedTime(std::optional<std::chrono::high_resolution_clock::time_point> &startTime)
{
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - startTime.value();
    std::cout << "time: " << elapsed.count() << std::endl;
    startTime.reset();
}

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
inline constexpr std::string_view LOGS_DIR{"logs"};
inline constexpr std::string_view SAVES_DIR{"mesh_saves"};
inline constexpr std::string_view PREPROCESSING_DIR{"preprocessing"};
inline constexpr std::string_view TPR_PREPROCESSING_DIR{"tprs"};
inline const std::string DEFAULT_FIRST_SAVE_DIR{"mesh_saves/save_0.hemesh"};
inline const char *MERGE_METRIC_IMG{"img/mergedMesh.png"};
inline const char *PREV_METRIC_IMG{"img/prevMesh.png"};
inline const char *ORIG_IMG{"img/origImage.png"};
inline const char *CURR_IMG{"img/currImage.png"};
inline const char *EDGE_MAP_IMG{"img/errorEdgeMap.png"};
inline constexpr int MAX_CURVE_DEPTH = 1000;

inline constexpr glm::vec3 blue{0.0f, 0.478f, 1.0f};
inline constexpr glm::vec3 yellow{1.0f, 0.9f, 0.0f};
inline constexpr glm::vec3 green{0.137f, 0.87f, 0.0f};
inline constexpr glm::vec3 black{0.0f};
inline constexpr glm::vec3 white{1.0f};
