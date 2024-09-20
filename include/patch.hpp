#pragma once

#include <bitset>
#include <cassert>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "types.hpp"

// The Patch class describes a single bicubic patch as a 4x4 control matrix
class Patch
{
public:
    Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild);

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

    const void updateHandleControlMatrix(int controlMatrixIdx, int mapHandleIdx, glm::vec2 geometricXY);
    const void updateHandlePointData(int cornerIdx, glm::vec2 cornerGeometricXY);

private:
    void populatePointData(std::bitset<4> isChild);

    std::vector<Vertex> controlMatrix = std::vector<Vertex>(16);
    std::vector<Vertex> pointData;
    std::vector<Vertex> handleData;
    std::vector<Vertex> pointHandleData;
    std::vector<Vertex> curveData = std::vector<Vertex>(12);
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
const void setPatchCoords(std::vector<Patch> &patches, PointId id, glm::vec2 newCoords);

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

inline constexpr std::array<int, 8> handleIndices = {1, 2, 4, 7, 8, 11, 13, 14};
inline constexpr std::array<int, 8> handleIdxs = {1, 4, 2, 7, 11, 14, 8, 13};

inline constexpr std::array<int, 12> cornerAndHandleIndices = {0, 3, 12, 15, 1, 2, 4, 7, 8, 11, 13, 14};

struct BezierConversionMap
{
    int parentPointIdx;
    float bezierConversionMultiplier;
};
constexpr std::array<BezierConversionMap, 8> convertToBezier = {
    BezierConversionMap{0, BCM},
    BezierConversionMap{3, -BCM},
    BezierConversionMap{0, BCM},
    BezierConversionMap{3, BCM},
    BezierConversionMap{12, -BCM},
    BezierConversionMap{15, -BCM},
    BezierConversionMap{12, BCM},
    BezierConversionMap{15, -BCM}};

constexpr std::array<std::array<int, 2>, 4> findMatchingHandles = {{{0, 2}, {1, 3}, {4, 6}, {5, 7}}};

constexpr std::array<int, 8> test = {0, 2, 1, 3, 4, 6, 5, 7};
