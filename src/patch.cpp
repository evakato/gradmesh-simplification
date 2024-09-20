#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix, std::bitset<4> isChild) : controlMatrix{controlMatrix}
{
    populatePointData(isChild);
}

void Patch::populatePointData(std::bitset<4> isChild)
{
    curveData[0] = controlMatrix[0];
    curveData[1] = controlMatrix[3];
    curveData[2] = controlMatrix[12];
    curveData[3] = controlMatrix[15];

    for (int i = 0; i < 8; i++)
    {
        BezierConversionMap map = convertToBezier[i];
        int handleIdx = handleIndices[i];
        Vertex &handleVertex = controlMatrix[handleIdx];
        glm::vec2 parentCoords = controlMatrix[map.parentPointIdx].coords;

        curveData[i + 4] = Vertex{hermiteToBezier(parentCoords, map.bezierConversionMultiplier, handleVertex.coords), handleVertex.color};
    }

    std::cout << isChild << std::endl;
    if (isChild.none())
    {
        for (std::size_t i = 0; i < 4; ++i)
        {
            pointData.push_back(controlMatrix[cornerIndices[i]]);
        }
        for (std::size_t i = 0; i < 8; i++)
        {
            BezierConversionMap map = convertToBezier[i];
            int handleIdx = handleIndices[i];
            Vertex &handleVertex = controlMatrix[handleIdx];
            glm::vec2 parentCoords = controlMatrix[map.parentPointIdx].coords;
            handleData.push_back(Vertex{hermiteToBezier(parentCoords, map.bezierConversionMultiplier, handleVertex.coords), handleVertex.color});
        }

        for (std::size_t i = 0; i < 4; ++i)
        {
            pointHandleData.push_back(controlMatrix[cornerIndices[i]]);
            for (std::size_t j = 0; j < 2; j++)
            {
                int matchingIdx = test[i * 2 + j];
                BezierConversionMap map = convertToBezier[matchingIdx];
                int handleIdx = handleIndices[matchingIdx];
                Vertex &handleVertex = controlMatrix[handleIdx];
                glm::vec2 parentCoords = controlMatrix[map.parentPointIdx].coords;
                pointHandleData.push_back(Vertex{hermiteToBezier(parentCoords, map.bezierConversionMultiplier, handleVertex.coords), handleVertex.color});
            }
        }
    }
}

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