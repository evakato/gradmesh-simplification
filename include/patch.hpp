#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "types.hpp"

using AABB = glm::vec4;

// The Patch class describes a single bicubic patch as a 4x4 control matrix
class Patch
{
public:
    Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild, std::vector<Vertex> pointData);

    Vertex &operator[](size_t index)
    {
        return controlMatrix[index];
    }
    Vertex operator()(int row, int col) const
    {
        assert(row >= 0 && row < 4);
        assert(col >= 0 && col < 4);
        return controlMatrix[row * 4 + col];
    }
    void setControlMatrix(size_t index, Vertex v) { controlMatrix[index] = v; }
    void setControlMatrix(std::vector<Vertex> newControlMatrix)
    {
        controlMatrix = newControlMatrix;
    }
    void setPointDataCoords(int i, glm::vec2 newCoords)
    {
        pointData[i].coords = newCoords;
    }
    const std::vector<Vertex> &getControlMatrix() const
    {
        return controlMatrix;
    }
    const std::vector<Vertex> &getPointData() const
    {
        return pointData;
    }
    const std::vector<Vertex> &getHandleData() const
    {
        return handleData;
    }
    const std::vector<Vertex> &getCurveData() const
    {
        return curveData;
    }
    const std::vector<Vertex> &getPointHandleData() const
    {
        return pointHandleData;
    }
    void setAABB()
    {
        glm::vec2 p0 = controlMatrix[0].coords;
        glm::vec2 p1 = controlMatrix[3].coords;
        glm::vec2 p2 = controlMatrix[12].coords;
        glm::vec2 p3 = controlMatrix[15].coords;
        float minX = std::min(std::min(std::min(p0.x, p1.x), p2.x), p3.x);
        float maxX = std::max(std::max(std::max(p0.x, p1.x), p2.x), p3.x);
        float minY = std::min(std::min(std::min(p0.y, p1.y), p2.y), p3.y);
        float maxY = std::max(std::max(std::max(p0.y, p1.y), p2.y), p3.y);
        aabb = AABB{minX, maxX, minY, maxY};
    }
    bool insideAABB(glm::vec2 pos) const
    {
        return (pos.x >= aabb.x && pos.x <= aabb.y) &&
               (pos.y >= aabb.z && pos.y <= aabb.w);
    }

    // const void updateHandleControlMatrix(int controlMatrixIdx, int mapHandleIdx, glm::vec2 geometricXY);
    // const void updateHandlePointData(int cornerIdx, glm::vec2 cornerGeometricXY);

private:
    void populatePointData(std::bitset<4> isChild);

    std::vector<Vertex> controlMatrix = std::vector<Vertex>(16);
    std::vector<Vertex> pointData;
    std::vector<Vertex> handleData;
    std::vector<Vertex> pointHandleData;
    std::vector<Vertex> curveData = std::vector<Vertex>(16);

    AABB aabb;
};

const std::vector<GLfloat> getGLCoordinates(std::vector<Vertex> vertices);

template <typename Func>
const std::vector<GLfloat> getAllPatchData(std::vector<Patch> &patches, Func func)
{

    std::vector<GLfloat> allPatchData;
    for (const auto &patch : patches)
    {
        std::vector<GLfloat> currPatchData = getGLCoordinates((patch.*func)());
        allPatchData.insert(allPatchData.end(), currPatchData.begin(), currPatchData.end());
    }
    return allPatchData;
}

// functions that operate on an array of patches
const PointId getSelectedPointId(const std::vector<Patch> &patches, glm::vec2 pos);
// const void setPatchCoords(std::vector<Patch> &patches, PointId id, glm::vec2 newCoords);
const int getSelectedPatch(const std::vector<Patch> &patches, glm::vec2 pos);

// some helper functions and maps to convert handles between bezier and hermite representation
// this is very messy
inline const glm::vec2 hermiteToBezier(glm::vec2 parentCoords, float multiplier, glm::vec2 tangentValues)
{
    return parentCoords + multiplier * tangentValues;
}
inline const glm::vec2 bezierToHermite(glm::vec2 tangentCoords, glm::vec2 parentCoords, float multiplier)
{
    return (tangentCoords - parentCoords) / multiplier;
}

inline constexpr std::array<int, 8> handleIndices = {1, 2, 7, 11, 14, 13, 8, 4};

struct BezierConversionMap
{
    int parentPointIdx;
    float bezierConversionMultiplier;
};

constexpr std::array<BezierConversionMap, 8> convertToBezier = {
    BezierConversionMap{0, BCM},
    BezierConversionMap{3, -BCM},
    BezierConversionMap{3, BCM},
    BezierConversionMap{15, -BCM},
    BezierConversionMap{15, -BCM},
    BezierConversionMap{12, BCM},
    BezierConversionMap{12, -BCM},
    BezierConversionMap{0, BCM}};

constexpr std::array<std::array<int, 2>, 4> findMatchingHandles = {{{0, 2}, {1, 3}, {4, 6}, {5, 7}}};
inline constexpr std::array<int, 12> cornerAndHandleIndices = {0, 3, 12, 15, 1, 2, 4, 7, 8, 11, 13, 14};