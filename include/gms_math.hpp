#pragma once

#include <tuple>

#include <glm/glm.hpp>

#include "types.hpp"

// glm is column-major
constexpr glm::mat4 hermiteBasisMat = glm::mat4(
    1.0f, 0.0f, -3.0f, 2.0f, // First column
    0.0f, 1.0f, -2.0f, 1.0f, // Second column
    0.0f, 0.0f, -1.0f, 1.0f, // Third column
    0.0f, 0.0f, 3.0f, -2.0f  // Fourth column
);

inline Vertex interpolateCubic(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(1.0f, t, t * t, t * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}

inline Vertex interpolateCubicDerivative(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(0.0f, 1.0f, 2.0f * t, 3.0f * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}

inline const glm::vec2 hermiteToBezier(glm::vec2 parentCoords, float multiplier, glm::vec2 tangentValues)
{
    return parentCoords + multiplier * tangentValues;
}
inline const glm::vec2 bezierToHermite(glm::vec2 tangentCoords, glm::vec2 parentCoords, float multiplier)
{
    return (tangentCoords - parentCoords) / multiplier;
}

inline glm::vec2 absSum(glm::vec2 coords1, glm::vec2 coords2)
{
    return {std::abs(coords1.x) + std::abs(coords2.x),
            std::abs(coords1.y) + std::abs(coords2.y)};
}

// reparameterization functions
inline auto parameterizeTBar2(float tRatio, const HalfEdge &relativeEdge)
{
    float otherBarParam = tRatio * relativeEdge.interval.y;
    float totalParam = 1.0f + otherBarParam;
    return std::make_tuple(otherBarParam, totalParam);
}
inline float totalCurveRelativeRight(float tRatio, const HalfEdge &relativeEdge, const HalfEdge &paramEdge)
{
    auto [bar1Param, unused] = parameterizeTBar2(tRatio, relativeEdge);
    float bar2Param = (paramEdge.interval.x / (1.0f - paramEdge.interval.x)) * bar1Param;
    return bar1Param + bar2Param + 1.0f;
}
inline auto parameterizeTBar1(float tRatio, const HalfEdge &relativeEdge)
{
    float otherBarParam = tRatio * (1.0f - relativeEdge.interval.x);
    float totalParam = 1.0f + otherBarParam;
    return std::make_tuple(otherBarParam, totalParam);
}
inline float totalCurveRelativeLeft(float tRatio, const HalfEdge &relativeEdge, const HalfEdge &paramEdge)
{
    auto [bar1Param, unused] = parameterizeTBar1(tRatio, relativeEdge);
    float bar2Param = (1.0f - paramEdge.interval.y) / paramEdge.interval.y * bar1Param;
    return bar1Param + bar2Param + 1.0f;
}

inline bool approximateFloating(float value, float eps = 1e-5)
{
    return std::abs(value) <= eps;
}

inline int getRandomInt(int max)
{
    return rand() % max;
}