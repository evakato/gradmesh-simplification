#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix) : controlMatrix{controlMatrix}
{
    populateCurveData();
}

void Patch::populateCurveData()
{
    curves.clear();
    for (int i = 0; i < 4; i++)
    {
        auto [m0, m0v, m1v, m0uv] = patchCurveIndices[i];
        Vertex p0 = Vertex{controlMatrix[m0].coords, black};
        Vertex p3 = Vertex{controlMatrix[m0uv].coords, black};

        Curve curve{p0, Vertex{hermiteToBezier(p0.coords, BCM, controlMatrix[m0v].coords), black}, Vertex{hermiteToBezier(p3.coords, -BCM, controlMatrix[m1v].coords), black}, p3, Curve::CurveType::Bezier};

        curves.push_back(curve);
        aabb.expand(curve.getAABB());
    }
}

void Patch::setCurveSelected(int curveIdx, glm::vec3 color)
{
    assert(curveIdx >= 0 && curveIdx < 4);
    curves[curveIdx].setColor(color);
}

int Patch::getContainingCurve(glm::vec2 pos) const
{
    for (size_t i = 0; i < 4; ++i)
    {
        auto curveAABB = curves[i].getAABB();
        curveAABB.addPadding(0.01f);
        if (curveAABB.contains(pos))
            return i;
    }
    return -1;
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
const std::vector<GLfloat> getAllPatchGLControlPointData(std::vector<Vertex> &points, std::optional<glm::vec3> color)
{
    std::vector<GLfloat> allPatchData;
    for (auto &point : points)
        vertexToGlData(allPatchData, point, color);
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
    for (size_t i = 0; i < patches.size(); ++i)
        if (patches[i].contains(pos))
            return i;
    return -1;
}
