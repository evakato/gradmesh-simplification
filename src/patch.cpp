#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild) : controlMatrix{controlMatrix}, isChild{isChild}
{
    populateCurveData();
    setAABB();
}

void Patch::populateCurveData()
{
    for (int i = 0; i < 4; i++)
    {
        std::array<int, 4> curveIdxs = patchCurveIndices[i];
        Vertex p0 = Vertex{controlMatrix[curveIdxs[0]].coords, black};
        Vertex p3 = Vertex{controlMatrix[curveIdxs[3]].coords, black};
        curveData[i * 4 + 0] = p0;
        curveData[i * 4 + 1] = Vertex{hermiteToBezier(p0.coords, BCM, controlMatrix[curveIdxs[1]].coords), black};
        curveData[i * 4 + 2] = Vertex{hermiteToBezier(p3.coords, -BCM, controlMatrix[curveIdxs[2]].coords), black};
        curveData[i * 4 + 3] = p3;
    }
}

const std::vector<GLfloat> getAllPatchGLPointData(std::vector<Patch> &patches, std::vector<int> idxs, std::optional<glm::vec3> color)
{
    std::vector<GLfloat> allPatchData;
    for (auto &patch : patches)
    {
        const std::vector<Vertex> &curveData = patch.getCurveData();
        for (int i = 0; i < 4; i++)
            if (!patch.curveIsChild(i))
                for (int idx : idxs)
                    vertexToGlData(allPatchData, curveData[i * 4 + idx], color);
    }
    return allPatchData;
}

void vertexToGlData(std::vector<GLfloat> &glData, Vertex v, std::optional<glm::vec3> color)
{
    glData.push_back(v.coords.x);
    glData.push_back(v.coords.y);
    if (color)
    {
        glData.push_back(color->x);
        glData.push_back(color->y);
        glData.push_back(color->z);
    }
    else
    {
        glData.push_back(v.color.x);
        glData.push_back(v.color.y);
        glData.push_back(v.color.z);
    }
}

void Patch::setCurveSelected(int curveIdx)
{
    assert(curveIdx >= 0 && curveIdx < 4);
    curveData[curveIdx * 4 + 0].color = blue;
    curveData[curveIdx * 4 + 1].color = blue;
    curveData[curveIdx * 4 + 2].color = blue;
    curveData[curveIdx * 4 + 3].color = blue;
}

void Patch::setAABB()
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

const int getSelectedPatch(const std::vector<Patch> &patches, glm::vec2 pos)
{
    for (std::size_t i = 0; i < patches.size(); ++i)
    {
        const auto &patch = patches[i];
        if (patch.insideAABB(pos))
            return i;
    }
    return -1;
}