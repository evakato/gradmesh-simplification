#include "curve.hpp"
#include "gms_math.hpp"
#include "ostream_ops.hpp"

Curve::Curve(Vertex v0, Vertex v1, Vertex v2, Vertex v3, CurveType curveType) : curveType{curveType}
{
    vertices.assign({v0, v1, v2, v3});
    if (curveType == CurveType::Bezier)
        aabb = bezierAABB(v0.coords, v1.coords, v2.coords, v3.coords);
    else if (curveType == CurveType::Hermite)
        aabb = hermiteAABB(v0.coords, v1.coords, v2.coords, v3.coords);
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

const std::vector<GLfloat> Curve::getGLPointData() const
{
    std::vector<GLfloat> vertexData;
    for (int i = 0; i < 4; i++)
    {
        const auto &vertex = vertices[i];
        if (curveType == CurveType::Hermite && i == 1)
        {
            auto newP1 = hermiteToBezier(vertices[0].coords, 1, vertex.coords);
            vertexData.push_back(newP1.x);
            vertexData.push_back(newP1.y);
        }
        else if (curveType == CurveType::Hermite && i == 2)
        {
            auto newP2 = hermiteToBezier(vertices[3].coords, 1, vertex.coords);
            vertexData.push_back(newP2.x);
            vertexData.push_back(newP2.y);
        }
        else
        {
            vertexData.push_back(vertex.coords.x);
            vertexData.push_back(vertex.coords.y);
        }
        vertexData.push_back(vertex.color.x);
        vertexData.push_back(vertex.color.y);
        vertexData.push_back(vertex.color.z);
    }
    return vertexData;
}

const std::vector<GLfloat> Curve::getGLAABBData() const
{
    auto col = blue;
    return {
        aabb.min.x, aabb.min.y, col[0], col[1], col[2],
        aabb.min.x, aabb.max.y, col[0], col[1], col[2],
        aabb.max.x, aabb.max.y, col[0], col[1], col[2],
        aabb.max.x, aabb.min.y, col[0], col[1], col[2]};
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
                return {i, j};
            }
        }
    }
    return {-1, -1};
}

void setCurveCoords(std::vector<Curve> &curves, PointId id, glm::vec2 newCoords)
{
    curves[id.primitiveId].setVertex(id.pointId, newCoords);
}

const glm::vec3 getColorAtPoint(const std::vector<Curve> &curves, PointId id)
{
    if (id.primitiveId == -1)
        return {1.0f, 1.0f, 1.0f};
    return curves[id.primitiveId].getColorAtVertex(id.pointId);
}

std::vector<Curve> getRandomCurves(Curve::CurveType curveType)
{
    glm::vec2 q1 = {-0.5f, -0.5f}; // Bottom-left corner
    glm::vec2 q2 = {0.5f, 0.5f};   // Top-right corner
    std::vector<std::vector<glm::vec2>> ranges = {
        {q1, {0.0f, -0.5f}},            // Left edge of the bottom-left quadrant
        {{-0.5f, 0.0f}, {0.5f, 0.0f}},  // Horizontal middle
        {{0.0f, -0.5f}, {0.5f, -0.5f}}, // Right edge of the bottom-left quadrant
        {{-0.5f, 0.5f}, {0.5f, 0.5f}},  // Top edge
    };
    std::vector<Curve> curves;
    for (auto range : ranges)
    {
        auto [col1, col2, col3, col4] = generateRandomCurveColors();
        auto [p0, p1, p2, p3] = generateRandomCurvePoints(range[0], range[1]);
        Vertex v1 = {p1, col2};
        Vertex v2 = {p2, col3};
        if (curveType == Curve::CurveType::Bezier)
        {
            v1 = Vertex{hermiteToBezier(p0, BCM, p1), col2};
            v2 = Vertex{hermiteToBezier(p3, -BCM, p2), col3};
        }
        Curve curve1{{p0, col1}, v1, v2, {p3, col4}, curveType};
        curves.push_back(curve1);
    }
    return curves;
}