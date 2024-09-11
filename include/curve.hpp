#pragma once

#include "types.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class Curve {
    public:
        Curve(Vertex v0, Vertex v1, Vertex v2, Vertex v3);
        //Curve(glm::vec2 pt1, glm::vec2 pt2, glm::vec2 pt3, glm::vec2 pt4);

        const std::vector<Vertex>& getVertices() const { return vertices; }
        const glm::vec3 getColorAtVertex(int i) const { return vertices[i].color; }

        void setVertex(int i, glm::vec2 coords) {
            vertices[i].coords = coords;
        }

        const std::vector<GLfloat> getGLVertexData() const;
        const std::vector<GLfloat> getGLVertexDataCol() const;

    private:
        std::vector<Vertex> vertices;
};

const std::vector<GLfloat> getAllGLVertexData(const std::vector<Curve>& curves);
const std::vector<GLfloat> getAllGLVertexDataCol(const std::vector<Curve> &curves);
const PointId getSelectedPointId(const std::vector<Curve>& curves, glm::vec2 pos);
const void setCurveCoords(std::vector<Curve>& curves, PointId id, glm::vec2 newCoords);
const glm::vec3 getColorAtPoint(const std::vector<Curve>& curves, PointId id);