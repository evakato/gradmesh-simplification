#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild) : controlMatrix{controlMatrix}, isChild{isChild}
{
    populateCurveData();
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

void Patch::setCurveSelected(int curveIdx, glm::vec3 color)
{
    assert(curveIdx >= 0 && curveIdx < 4);
    curveData[curveIdx * 4 + 0].color = color;
    curveData[curveIdx * 4 + 1].color = color;
    curveData[curveIdx * 4 + 2].color = color;
    curveData[curveIdx * 4 + 3].color = color;
}

const std::vector<GLfloat> getAllHandleGLPoints(const std::vector<Vertex> &handles, int firstIdx, int step)
{
    std::vector<GLfloat> allHandleData;
    if (handles.size() > 0)
    {
        for (int i = firstIdx; i < handles.size(); i += step)
        {
            auto &handle = handles[i];
            vertexToGlData(allHandleData, handle);
        }
    }
    return allHandleData;
}
const std::vector<GLfloat> getAllPatchGLControlPointData(std::vector<Patch> &patches, std::optional<glm::vec3> color)
{
    std::vector<GLfloat> allPatchData;
    for (auto &patch : patches)
    {
        const std::vector<Vertex> &controlMatrix = patch.getControlMatrix();
        for (int i = 0; i < 4; i++)
            if (!patch.curveIsChild(i))
                vertexToGlData(allPatchData, controlMatrix[controlPointIdxs[i]], color);
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

int getSelectedPatch(const std::vector<Patch> &patches, glm::vec2 pos)
{
    // got rid of AABB stuff in this class so this function died (for now)
    return -1;
}
