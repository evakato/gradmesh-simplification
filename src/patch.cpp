#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild, std::vector<Vertex> pointData) : controlMatrix{controlMatrix}, pointData{pointData}
{
    populatePointData(isChild);
    setAABB();
}

void Patch::populatePointData(std::bitset<4> isChild)
{
    for (int i = 0; i < VERTS_PER_CURVE; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            BezierConversionMap map = convertToBezier[i * 2 + j];
            int handleIdx = handleIndices[i * 2 + j];
            Vertex &originalHandle = controlMatrix[handleIdx];
            Vertex parentControlPoint = controlMatrix[map.parentPointIdx];
            Vertex convertedHandle = Vertex{hermiteToBezier(parentControlPoint.coords, map.bezierConversionMultiplier, originalHandle.coords), originalHandle.color};
            if (j == 0)
            {
                curveData[i * 4] = parentControlPoint;
                curveData[i * 4 + 1] = convertedHandle;
            }
            else
            {
                curveData[i * 4 + 3] = parentControlPoint;
                curveData[i * 4 + 2] = convertedHandle;
            }

            if (!isChild.test(i))
            {
                handleData.push_back(convertedHandle);
                pointHandleData.push_back(parentControlPoint);
                pointHandleData.push_back(convertedHandle);
            }
        }
    }
}

/*
const void Patch::updateHandleControlMatrix(int controlMatrixIdx, int mapHandleIdx, glm::vec2 geometricXY)
{
    BezierConversionMap map = convertToBezier[mapHandleIdx]; // mapHandleIdx is [0,8]
    Vertex &parentVertex = controlMatrix[map.parentPointIdx];
    controlMatrix[controlMatrixIdx].coords = bezierToHermite(geometricXY, parentVertex.coords, map.bezierConversionMultiplier);
}

const void Patch::updateHandlePointData(int cornerIdx, glm::vec2 cornerGeometricXY)
{
    const std::array<int, 2> matchingHandleIdxs = findMatchingHandles[cornerIdx];
    for (int i = 0; i < 2; i++)
    {
        int conversionIdx = matchingHandleIdxs[i];
        BezierConversionMap map = convertToBezier[conversionIdx];
        Vertex &handleVertex = controlMatrix[handleIndices[conversionIdx]];
        pointData[conversionIdx + 4].coords = hermiteToBezier(cornerGeometricXY, map.bezierConversionMultiplier, handleVertex.coords);
    }
}
*/

const PointId getSelectedPointId(const std::vector<Patch> &patches, glm::vec2 pos)
{
    for (int i = 0; i < patches.size(); ++i)
    {
        const auto &pointData = patches[i].getPointData();
        for (int j = 0; j < 12; j++)
        {
            if (glm::distance(pos, pointData[j].coords) <= 0.03f)
            {
                return {i, j};
            }
        }
    }
    return {-1, -1};
}

/*
const void setPatchCoords(std::vector<Patch> &patches, PointId id, glm::vec2 newCoords)
{
    Patch &currentPatch = patches[id.primitiveId];
    const int controlMatrixIdx = cornerAndHandleIndices[id.pointId];
    currentPatch.setPointDataCoords(id.pointId, newCoords);

    // if selected point is a handle, we have to calculate the tangent from the geometric data
    if (id.pointId >= 4)
    {
        currentPatch.updateHandleControlMatrix(controlMatrixIdx, id.pointId - 4, newCoords);
    }
    // else if selected point is a corner, we have to recalculate the positions of the tangents
    else
    {
        currentPatch.updateHandlePointData(id.pointId, newCoords);
        currentPatch[controlMatrixIdx].coords = newCoords;
    }
}
*/

const std::vector<GLfloat> getGLCoordinates(std::vector<Vertex> vertices)
{
    std::vector<GLfloat> coordinateData;
    for (const auto &point : vertices)
    {
        coordinateData.push_back(point.coords.x);
        coordinateData.push_back(point.coords.y);
        coordinateData.push_back(point.color.x);
        coordinateData.push_back(point.color.y);
        coordinateData.push_back(point.color.z);
    }
    return coordinateData;
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