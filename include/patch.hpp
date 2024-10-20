#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <optional>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "types.hpp"
#include "gms_math.hpp"

struct AABB
{
    glm::vec2 min;
    glm::vec2 max;

    void expand(const AABB &other)
    {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
};

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
    const std::vector<Vertex> &getControlMatrix() const
    {
        return controlMatrix;
    }
    const std::vector<Vertex> &getCurveData() const
    {
        return curveData;
    }
    const bool curveIsChild(int idx) const { return isChild.test(idx); }

    void setControlMatrix(size_t index, Vertex v) { controlMatrix[index] = v; }
    void setControlMatrix(std::vector<Vertex> newControlMatrix)
    {
        controlMatrix = newControlMatrix;
    }

    void setAABB();
    bool insideAABB(glm::vec2 pos) const
    {
        return (pos.x >= aabb.min.x && pos.x <= aabb.max.x) &&
               (pos.y >= aabb.min.y && pos.y <= aabb.max.y);
    }
    AABB getAABB() const { return aabb; }

    void setCurveSelected(int curveIdx, glm::vec3 color);

private:
    void populateCurveData();

    std::vector<Vertex> controlMatrix = std::vector<Vertex>(16);
    std::vector<Vertex> curveData = std::vector<Vertex>(16);
    std::bitset<4> isChild;

    AABB aabb;
};

void vertexToGlData(std::vector<GLfloat> &glData, Vertex v, std::optional<glm::vec3> color = std::nullopt);

// functions that operate on an array of patches
template <typename Func>
const std::vector<GLfloat> getAllPatchGLData(const std::vector<Patch> &patches, Func func)
{
    std::vector<GLfloat> allPatchData;
    for (auto &patch : patches)
    {
        const std::vector<Vertex> &curveData = (patch.*func)();
        for (const Vertex &v : curveData)
        {
            vertexToGlData(allPatchData, v);
        }
    }
    return allPatchData;
}
const std::vector<GLfloat> getAllHandleGLPoints(const std::vector<Vertex> &handles, int firstIdx = 0, int step = 1);
const std::vector<GLfloat> getAllPatchGLControlPointData(std::vector<Patch> &patches, std::optional<glm::vec3> color);
int getSelectedPatch(const std::vector<Patch> &patches, glm::vec2 pos);
AABB getMeshAABB(const std::vector<Patch> &patches);

using Int4x4 = std::array<std::array<int, 4>, 4>;
inline constexpr Int4x4 patchCurveIndices = {{{0, 1, 2, 3},
                                              {3, 7, 11, 15},
                                              {12, 13, 14, 15},
                                              {0, 4, 8, 12}}};

inline constexpr std::array<int, 4> controlPointIdxs = {0, 3, 15, 12};

constexpr std::array<std::array<int, 2>, 4>
    findMatchingHandles = {{{0, 2}, {1, 3}, {4, 6}, {5, 7}}};
inline constexpr std::array<int, 12> cornerAndHandleIndices = {0, 3, 12, 15, 1, 2, 4, 7, 8, 11, 13, 14};