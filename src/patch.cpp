#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix) : controlMatrix{controlMatrix}
{
    populateCurveData({-1, -1, -1, -1});
}

Patch::Patch(std::vector<Vertex> controlMatrix, int faceIdx, std::array<int, 4> halfEdgeIdxs) : controlMatrix{controlMatrix}, faceIdx{faceIdx}
{
    populateCurveData(halfEdgeIdxs);
}

void Patch::populateCurveData(std::array<int, 4> halfEdgeIdxs)
{
    curves.clear();
    for (int i = 0; i < 4; i++)
    {
        auto [m0, m0v, m1v, m0uv] = patchCurveIndices[i];
        Vertex p0 = Vertex{controlMatrix[m0].coords, black};
        Vertex p3 = Vertex{controlMatrix[m0uv].coords, black};

        Curve curve{p0,                                                                         //
                    Vertex{hermiteToBezier(p0.coords, BCM, controlMatrix[m0v].coords), black},  //
                    Vertex{hermiteToBezier(p3.coords, -BCM, controlMatrix[m1v].coords), black}, //
                    p3,                                                                         //
                    Curve::CurveType::Bezier, halfEdgeIdxs[i]};

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
    std::vector<int> curveIdxs;
    for (size_t i = 0; i < 4; ++i)
    {
        auto curveAABB = curves[i].getAABB();
        curveAABB.addPadding(0.01f);
        if (curveAABB.contains(pos))
            curveIdxs.push_back(i);
    }
    if (curveIdxs.empty())
        return -1;

    float min_dist = 1;
    int selectedCurveIdx = -1;
    for (int curveIdx : curveIdxs)
    {
        float dist = curves[curveIdx].distanceToCurve(pos);
        if (min_dist > dist)
        {
            selectedCurveIdx = curveIdx;
            min_dist = dist;
        }
    }
    return selectedCurveIdx;
}

Vertex Patch::findPatchPoint(float u, float v) const
{
    glm::vec4 vVec = glm::vec4(1.0f, v, v * v, v * v * v) * hermiteBasisMat;
    glm::vec4 uVec = glm::vec4(1.0f, u, u * u, u * u * u) * hermiteBasisMat;
    std::array<Vertex, 4> cmXv;
    for (int row = 0; row < 4; ++row)
    {
        cmXv[row] = controlMatrix[row * 4] * vVec[0] +
                    controlMatrix[row * 4 + 1] * vVec[1] +
                    controlMatrix[row * 4 + 2] * vVec[2] +
                    controlMatrix[row * 4 + 3] * vVec[3];
    }
    return cmXv[0] * uVec[0] + cmXv[1] * uVec[1] + cmXv[2] * uVec[2] + cmXv[3] * uVec[3];
}

double Patch::isPointInsidePatch(const glm::vec2 &P, double tolerance) const
{
    double best_u = 0.0, best_v = 0.0;
    double min_dist = std::numeric_limits<double>::infinity();

    const int steps = 50; // Increase for better precision
    for (int i = 0; i <= steps; ++i)
    {
        for (int j = 0; j <= steps; ++j)
        {
            double u = i / static_cast<double>(steps);
            double v = j / static_cast<double>(steps);

            glm::vec2 p_uv = findPatchPoint(u, v).coords;
            double dist = glm::distance(p_uv, P);

            if (dist < min_dist)
            {
                min_dist = dist;
                best_u = u;
                best_v = v;
            }
        }
    }
    if (best_u >= 0 && best_u <= 1 && best_v >= 0 && best_v <= 1)
        return min_dist;
    return 1;
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
    std::vector<int> patchIdxs;
    for (size_t i = 0; i < patches.size(); ++i)
        if (patches[i].contains(pos))
            patchIdxs.push_back(i);

    if (patchIdxs.empty())
        return -1;

    double min_dist = 1;
    int selectedPatchIdx = -1;
    for (int patchIdx : patchIdxs)
    {
        double dist = patches[patchIdx].isPointInsidePatch(pos);
        if (min_dist > dist)
        {
            selectedPatchIdx = patchIdx;
            min_dist = dist;
        }
    }
    return selectedPatchIdx;
}

int getPatchFromFaceIdx(const std::vector<Patch> &patches, int faceIdx)
{
    for (size_t i = 0; i < patches.size(); ++i)
        if (patches[i].getFaceIdx() == faceIdx)
            return i;
    return -1;
}

CurveId getCurveIdFromEdgeIdx(const std::vector<Patch> &patches, int targetEdgeIdx)
{
    for (size_t i = 0; i < patches.size(); ++i)
    {
        const auto allHalfEdgeIdxs(patches[i].getCurves() //
                                   | std::views::transform([](const Curve &curve)
                                                           { return curve.getHalfEdgeIdx(); }));

        auto it = std::ranges::find(allHalfEdgeIdxs, targetEdgeIdx);
        if (it != allHalfEdgeIdxs.end())
        {
            return CurveId{static_cast<int>(i), static_cast<int>(std::distance(allHalfEdgeIdxs.begin(), it))};
        }
    }
    return CurveId{-1, -1};
}