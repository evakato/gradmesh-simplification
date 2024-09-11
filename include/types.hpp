#pragma once

#include <utility>
#include <vector>

#include <glm/glm.hpp>

struct PointId
{
    int primitiveId;
    int pointId;
};

struct Vertex
{
    glm::vec2 coords;
    glm::vec3 color;
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

inline constexpr int VERTS_PER_PATCH{16};
inline constexpr int VERTS_PER_CURVE{4};
inline constexpr int SCR_WIDTH{1280};
inline constexpr int SCR_HEIGHT{920};
inline constexpr int GUI_WIDTH{SCR_WIDTH - SCR_HEIGHT};
inline constexpr int GUI_POS{SCR_WIDTH - GUI_WIDTH};

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

inline constexpr unsigned int cmi[][4] = {
    {0, 1, 2, 5},
    {3, 7, 11, 6},
    {15, 14, 13, 10},
    {12, 8, 4, 9},
};
inline constexpr float tw[][2] = {
    {1, -1},
    {1, -1},
    {-1, 1},
    {-1, 1},
};

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
inline std::vector<unsigned int> generateHandleEBO(int numPatches)
{
    std::vector<unsigned int> indices;
    for (int set = 0; set < numPatches; ++set)
    {
        int baseIndex = set * 12;
        for (int i = 0; i < 16; ++i)
            indices.push_back(handleIndicesForPatch[i] + baseIndex);
    }
    return indices;
}