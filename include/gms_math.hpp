#pragma once

#include <chrono>
#include <ranges>
#include <random>
#include <algorithm>
#include <optional>
#include <stdlib.h>
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
constexpr glm::mat4 bezierBasisMat = glm::mat4(
    1.0f, -3.0f, 3.0f, -1.0f, // First column
    0.0f, 3.0f, -6.0f, 3.0f,  // Second column
    0.0f, 0.0f, 3.0f, -3.0f,  // Third column
    0.0f, 0.0f, 0.0f, 1.0f    // Fourth column
);

inline Vertex interpolateCubic(CurveVector curve, float t)
{
    glm::vec4 tVec = glm::vec4(1.0f, t, t * t, t * t * t) * hermiteBasisMat;
    return curve[0] * tVec[0] + curve[1] * tVec[1] + curve[2] * tVec[2] + curve[3] * tVec[3];
}

inline glm::vec2 interpolateCubic(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float t, const glm::mat4 basisMat)
{
    glm::vec4 tVec = glm::vec4(1.0f, t, t * t, t * t * t) * basisMat;
    return p0 * tVec[0] + p1 * tVec[1] + p2 * tVec[2] + p3 * tVec[3];
}

using PointVector = std::array<glm::vec2, 4>;
inline PointVector applyCubicBasis(PointVector p, const glm::mat4 &basisMat)
{
    glm::vec4 px(p[0].x, p[1].x, p[2].x, p[3].x);
    glm::vec4 py(p[0].y, p[1].y, p[2].y, p[3].y);
    glm::vec4 transformedX = basisMat * px;
    glm::vec4 transformedY = basisMat * py;
    PointVector result = {
        glm::vec2(transformedX.x, transformedY.x),
        glm::vec2(transformedX.y, transformedY.y),
        glm::vec2(transformedX.z, transformedY.z),
        glm::vec2(transformedX.w, transformedY.w)};
    return result;
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

inline int getRandomInt(auto seed, int max)
{
    if (max == 0)
        return 0;

    std::srand(static_cast<unsigned int>(seed));
    return std::rand() % max;
}

inline std::vector<int> generateRandomNums(int N)
{
    std::vector<int> numbers(N + 1);
    std::iota(numbers.begin(), numbers.end(), 0);
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 gen(seed);
    std::shuffle(numbers.begin(), numbers.end(), gen);
    return numbers;
}

AABB bezierAABB(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3);
AABB hermiteAABB(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3);

inline std::array<glm::vec2, 4> generateRandomCurvePoints(glm::vec2 rangeP0, glm::vec2 rangeP3)
{
    std::array<glm::vec2, 4> points;
    std::random_device rd;  // Obtain a random number generator
    std::mt19937 eng(rd()); // Seed the generator

    // Distributions for the corners
    std::uniform_real_distribution<float> distX(rangeP0.x, rangeP3.x);
    std::uniform_real_distribution<float> distY(rangeP0.y, rangeP3.y);
    std::uniform_real_distribution<float> tangentDist(-1.0f, 1.0f);

    // Generate control points
    points[0] = {distX(eng), distY(eng)}; // p0
    points[3] = {distX(eng), distY(eng)}; // p3

    points[1] = {tangentDist(eng), tangentDist(eng)}; // p1
    points[2] = {tangentDist(eng), tangentDist(eng)}; // p2

    return points; // Return the array of points
}

inline std::array<glm::vec3, 4> generateRandomCurveColors()
{
    std::array<glm::vec3, 4> colors;
    std::random_device rd;                                  // Obtain a random number generator
    std::mt19937 eng(rd());                                 // Seed the generator
    std::uniform_real_distribution<float> dist(0.0f, 1.0f); // Define the range for components

    for (int i = 0; i < 4; ++i)
    {
        // Ensure at least one component is greater than 0.5 for brightness
        float r = dist(eng);
        float g = dist(eng);
        float b = dist(eng);

        // Make sure one channel is always bright
        if (r < 0.5f)
            r += 0.5f;
        if (g < 0.5f)
            g += 0.5f;
        if (b < 0.5f)
            b += 0.5f;

        colors[i] = {r, g, b}; // Assign the bright color
    }

    return colors; // Return the array of bright colors
}
