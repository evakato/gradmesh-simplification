#include "curve.hpp"

Curve::Curve(Vertex v0, Vertex v1, Vertex v2, Vertex v3)
{
    vertices.assign({v0, v1, v2, v3});
}

const std::vector<GLfloat> Curve::getGLVertexData() const
{
    std::vector<GLfloat> vertexData;
    for (const auto &vertex : vertices)
    {
        vertexData.push_back(vertex.coords.x);
        vertexData.push_back(vertex.coords.y);
    }
    return vertexData;
}

const std::vector<GLfloat> Curve::getGLVertexDataCol() const
{
    std::vector<GLfloat> vertexData;
    for (const auto &vertex : vertices)
    {
        vertexData.push_back(vertex.coords.x);
        vertexData.push_back(vertex.coords.y);
        vertexData.push_back(vertex.color.x);
        vertexData.push_back(vertex.color.y);
        vertexData.push_back(vertex.color.z);
    }
    return vertexData;
}

const std::vector<GLfloat> getAllGLVertexData(const std::vector<Curve> &curves)
{
    std::vector<GLfloat> vertexData;
    for (auto &curve : curves)
    {
        std::vector<GLfloat> curveVertexData = curve.getGLVertexData();
        vertexData.insert(vertexData.end(), curveVertexData.begin(), curveVertexData.end());
    }
    return vertexData;
}

const std::vector<GLfloat> getAllGLVertexDataCol(const std::vector<Curve> &curves)
{
    std::vector<GLfloat> vertexData;
    for (auto &curve : curves)
    {
        std::vector<GLfloat> curveVertexData = curve.getGLVertexDataCol();
        vertexData.insert(vertexData.end(), curveVertexData.begin(), curveVertexData.end());
    }
    return vertexData;
}

const PointId getSelectedPointId(const std::vector<Curve> &curves, glm::vec2 pos)
{
    for (int i = 0; i < curves.size(); ++i)
    {
        const auto &vertices = curves[i].getVertices();
        for (int j = 0; j < 4; ++j)
        {
            if (glm::distance(pos, vertices[j].coords) <= 0.03f)
            {
                return { i, j };
            }
        }
    }
    return { -1, -1 };
}

const void setCurveCoords(std::vector<Curve>& curves, PointId id, glm::vec2 newCoords) {
    curves[id.primitiveId].setVertex(id.pointId, newCoords);
}

const glm::vec3 getColorAtPoint(const std::vector<Curve>& curves, PointId id) {
    if (id.primitiveId == -1) return { 1.0f, 1.0f, 1.0f };
    return curves[id.primitiveId].getColorAtVertex(id.pointId);
}