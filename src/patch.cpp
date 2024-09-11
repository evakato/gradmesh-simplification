#include "patch.hpp"

Patch::Patch(std::vector<Vertex> controlMatrix) : controlMatrix{controlMatrix} {}

Patch::Patch(std::vector<glm::vec2> points, std::vector<glm::vec3> cols)
{
    for (size_t i = 0; i < points.size(); ++i)
    {
        controlMatrix.push_back(Vertex{points[i], cols[i]});
    }
}

Patch::Patch(std::vector<Vertex> corners, std::vector<Vertex> handles)
{
    controlMatrix[0] = corners[0];
    controlMatrix[1] = handles[0];
    controlMatrix[2] = handles[1];
    controlMatrix[3] = corners[1];
    controlMatrix[4] = handles[2];
    controlMatrix[5] = {glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    controlMatrix[6] = {glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    controlMatrix[7] = handles[3];
    controlMatrix[8] = handles[4];
    controlMatrix[9] = {glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    controlMatrix[10] = {glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    controlMatrix[11] = handles[5];
    controlMatrix[12] = corners[2];
    controlMatrix[13] = handles[6];
    controlMatrix[14] = handles[7];
    controlMatrix[15] = corners[3];
    populatePointData();
}

Patch::Patch(std::vector<Vertex> corners, std::vector<Vertex> handles, std::vector<Vertex> twists)
{
    controlMatrix[0] = corners[0];
    controlMatrix[1] = handles[0];
    controlMatrix[2] = handles[1];
    controlMatrix[3] = corners[1];
    controlMatrix[4] = handles[2];
    controlMatrix[5] = twists[0];
    controlMatrix[6] = twists[1];
    controlMatrix[7] = handles[3];
    controlMatrix[8] = handles[4];
    controlMatrix[9] = twists[2];
    controlMatrix[10] = twists[3];
    controlMatrix[11] = handles[5];
    controlMatrix[12] = corners[2];
    controlMatrix[13] = handles[6];
    controlMatrix[14] = handles[7];
    controlMatrix[15] = corners[3];
    populatePointData();
}

const void Patch::populatePointData()
{
    pointData[0] = controlMatrix[0];
    pointData[1] = controlMatrix[3];
    pointData[2] = controlMatrix[12];
    pointData[3] = controlMatrix[15];

    for (int i = 0; i < 8; i++)
    {
        BezierConversionMap map = convertToBezier[i];
        int handleIdx = handleIndices[i];
        Vertex &handleVertex = controlMatrix[handleIdx];
        glm::vec2 parentCoords = controlMatrix[map.parentPointIdx].coords;

        pointData[i + 4] = Vertex{hermiteToBezier(parentCoords, map.bezierConversionMultiplier, handleVertex.coords), handleVertex.color};
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

const std::vector<GLfloat> Patch::getGLControlMatrixData() const
{
    std::vector<GLfloat> controlMatrixData;
    for (const auto &element : controlMatrix)
    {
        controlMatrixData.push_back(element.coords.x);
        controlMatrixData.push_back(element.coords.y);
        controlMatrixData.push_back(element.color.x);
        controlMatrixData.push_back(element.color.y);
        controlMatrixData.push_back(element.color.z);
    }
    return controlMatrixData;
}

const std::vector<GLfloat> Patch::getGLCoordinates() const
{
    std::vector<GLfloat> coordinateData;
    for (const auto &point : pointData)
    {
        coordinateData.push_back(point.coords.x);
        coordinateData.push_back(point.coords.y);
        coordinateData.push_back(point.color.x);
        coordinateData.push_back(point.color.y);
        coordinateData.push_back(point.color.z);
    }
    return coordinateData;
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

const std::vector<GLfloat> getAllPatchVertexData(std::vector<Patch> &patches)
{
    std::vector<GLfloat> allPatchVertexData;
    for (const auto &patch : patches)
    {
        std::vector<GLfloat> currPatchVertexData = patch.getGLControlMatrixData();
        allPatchVertexData.insert(allPatchVertexData.end(), currPatchVertexData.begin(), currPatchVertexData.end());
    }
    return allPatchVertexData;
}

const std::vector<GLfloat> getAllPatchCoordData(std::vector<Patch> &patches)
{
    std::vector<GLfloat> allPatchCoordData;
    for (const auto &patch : patches)
    {
        std::vector<GLfloat> currPatchVertexData = patch.getGLCoordinates();
        allPatchCoordData.insert(allPatchCoordData.end(), currPatchVertexData.begin(), currPatchVertexData.end());
    }
    return allPatchCoordData;
}