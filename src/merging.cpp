#include "gradmesh.hpp"

namespace Merging
{
    void merge(GradMesh &mesh)
    {
        std::vector<int> candidateEdgeList = candidateEdges(mesh, mesh.faces);
        std::cout << "There are " << candidateEdgeList.size() << " possible merges.";
        int selectedEdge = chooseEdge(candidateEdgeList);
        std::cout << "selected edge: " << selectedEdge << std::endl;
        if (selectedEdge != -1)
            mergePatches(mesh, selectedEdge);
        else
            std::cout << "Merging not possible\n";
    }

    int chooseEdge(const std::vector<int> &candidateEdgeList)
    {
        if (candidateEdgeList.size() > 0)
            return candidateEdgeList[0];
        return -1;
    }

    std::vector<int> candidateEdges(const GradMesh &mesh, const std::vector<Face> &faces)
    {
        std::vector<int> candidateEdgeList = {};
        for (auto &face : faces)
        {
            if (face.halfEdgeIdx != -1)
            {
                int currIdx = face.halfEdgeIdx;
                for (int i = 0; i < 4; i++)
                {
                    const auto &currEdge = mesh.getEdge(currIdx);
                    if (currEdge.twinIdx != -1 && !currEdge.isBar() && !mesh.halfEdgeIsParent(currEdge.twinIdx))
                    {
                        candidateEdgeList.push_back(currIdx);
                    }
                    currIdx = currEdge.nextIdx;
                }
            }
        }
        return candidateEdgeList;
    }

    void copyNormalHalfEdge(GradMesh &mesh, HalfEdge &e1, HalfEdge &e2)
    {
        mesh.disablePoint(e1.originIdx);
        e1.originIdx = e2.originIdx;
        e1.color = e2.color;
        e1.twist = e2.twist;
    }

    void scaleHandles(Handle &h1, Handle &h2, Handle &f2h, float t)
    {
        float leftMul = 1.0f / t;
        float rightMul = 1.0f / (1.0f - t);
        h1 *= leftMul;
        h2 = f2h * rightMul;
    }

    bool removeWholeTJunction(HalfEdge &bar1, HalfEdge &bar2)
    {
        bar1.interval.x = std::min(bar1.interval.x, bar2.interval.x);
        bar1.interval.y = std::max(bar1.interval.y, bar2.interval.y);
        // TODO : replace with approximations
        return (bar1.interval.x == 0 && bar1.interval.y == 1);
    }

    void mergePatches(GradMesh &mesh, int mergeEdgeIdx)
    {
        auto &face1e1 = mesh.edges[mergeEdgeIdx];
        auto &face2e1 = mesh.edges[face1e1.twinIdx];
        int face1e2Idx = face1e1.nextIdx;
        auto &face1e2 = mesh.edges[face1e2Idx];
        auto &face1e3 = mesh.edges[face1e2.nextIdx];
        int face1e4Idx = face1e3.nextIdx;
        auto &face1e4 = mesh.edges[face1e4Idx];
        auto &face2e2 = mesh.edges[face2e1.nextIdx];
        auto &face2e3 = mesh.edges[face2e2.nextIdx];
        auto &face2e4 = mesh.edges[face2e3.nextIdx];

        if (face1e1.isNormal())
        {
            std::cout << "Creating a merged patch at a normal half-edge f1e1\n";
            if (face2e2.isBar())
            {
                std::cout << "f2e2 is a bar.\n";
            }
            if (face1e4.isBar())
            {
                std::cout << "f1e4 is a bar.\n";
            }

            copyNormalHalfEdge(mesh, face1e1, face2e3);
            face1e1.handleIdxs = face2e3.handleIdxs;
            const float t = splittingFactor(mesh, face1e1, face1e2, face2e2, -1);
            auto &face1e4h1 = mesh.handles[face1e4.handleIdxs.first];
            auto &face1e4h2 = mesh.handles[face1e4.handleIdxs.second];
            auto &face2e2h2 = mesh.handles[face2e2.handleIdxs.second];
            scaleHandles(face1e4h1, face1e4h2, face2e2h2, t);

            // Adding a t-junction: check that these edges have twins
            if (face1e2.twinIdx != -1 && face2e4.twinIdx != -1)
            {
                int stemChild = mesh.edges[face1e2.twinIdx].nextIdx;
                mesh.addTJunction(face1e2.twinIdx, face2e4.twinIdx, stemChild, face1e2Idx, t);
            }
        }
        else if (face1e1.isStem())
        {
            std::cout << "Creating a merged patch at a stem half-edge f1e1\n";
            if (removeWholeTJunction(face1e4, face2e2))
            {
                mesh.replaceChildWithParent(face1e4Idx);
                copyNormalHalfEdge(mesh, face1e1, face2e3);
                face1e1.parentIdx = -1;
            }
            else
            {
                face1e1.interval = face2e3.interval;
                face1e1.handleIdxs = face2e3.handleIdxs;
                copyNormalHalfEdge(mesh, face1e1, face2e3);
                face1e1.parentIdx = face2e3.parentIdx;
            }
        }

        if (face2e1.isNormal())
        {
            std::cout << "Creating a merged patch at a normal half-edge f2e1\n";
            copyNormalHalfEdge(mesh, face1e2, face2e4);
            if (face2e4.isBar())
            {
                std::cout << "f2e4 is a bar.\n";
            }
            if (face1e2.isBar())
            {
                std::cout << "f1e2 is a bar.\n";
            }
            const float t = splittingFactor(mesh, face2e1, face1e2, face2e4, 1);
            auto &face1e2h1 = mesh.handles[face1e2.handleIdxs.first];
            auto &face1e2h2 = mesh.handles[face1e2.handleIdxs.second];
            auto &face2e4h1 = mesh.handles[face2e4.handleIdxs.first];
            scaleHandles(face1e2h2, face1e2h1, face2e4h1, t);

            // Adding a t-junction
            if (face1e4.twinIdx != -1 && face2e2.twinIdx != -1)
            {
                int stemChild = mesh.edges[face1e4.twinIdx].nextIdx;
                mesh.addTJunction(face1e4.twinIdx, face2e2.twinIdx, stemChild, face1e2Idx, t);
            }
        }
        else if (face2e1.isStem())
        {
            std::cout << "Creating a merged patch at a stem half-edge f2e1\n";
            if (removeWholeTJunction(face1e2, face2e4))
            {
                mesh.replaceChildWithParent(face1e2Idx);
            }
            else
            {
                face1e2.color = face2e4.color;
            }
        }

        face1e1.twinIdx = face2e3.twinIdx;
        if (face2e3.twinIdx != -1)
            mesh.edges[face2e3.twinIdx].twinIdx = mergeEdgeIdx;
        std::cout << "patch removing: " << face2e1.patchIdx << std::endl;
        mesh.faces[face2e1.patchIdx].halfEdgeIdx = -1;
        face2e1.patchIdx = -1;
        face2e2.patchIdx = -1;
        face2e3.patchIdx = -1;
        face2e4.patchIdx = -1;
    }

    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign)
    {
        // im only using the geometric data rn, idk if i should include color
        const auto &leftPv01 = mesh.getTangent(bar1, 1); // left patch: P_v(0,1)
        // const auto &leftPv11 = mesh.getTangent(face1e2, 0);  // left patch: P_v(1,1)
        const auto &rightPv00 = mesh.getTangent(bar2, 0); // right patch: P_v(0,0)
        // const auto &rightPv10 = mesh.getTangent(face2e4, 1); // right patch: P_v(1,0)
        const glm::vec2 &leftPuv01 = stem.twist.coords; // left patch: P_uv(0,1)
        // const glm::vec2 &leftPuv11 = face1e2.twist.coords;  // left patch: P_uv(1,1)
        const glm::vec2 &rightPuv00 = bar2.twist.coords; // right patch: P_uv(0,0)
        // const glm::vec2 &rightPuv10 = face2e1.twist.coords; // right patch: P_uv(1,0)

        glm::vec2 sumPv0 = absSum(leftPv01.coords, rightPv00.coords); // P_v(0,1) + P_v(0,0)
        // glm::vec2 sumPv1 = absSum(leftPv11.coords, rightPv10.coords); // P_v(1,1) + P_v(1,0)

        float r1 = glm::length(leftPv01.coords) / glm::length(sumPv0);
        float r2 = glm::length(absSum(leftPv01.coords, sign * BCM * leftPuv01)) / glm::length(sumPv0 + sign * BCM * absSum(leftPuv01, rightPuv00));
        // r3 = glm::length(absSum(leftPv11.coords, BCM * leftPuv11)) / glm::length(sumPv1 + BCM * absSum(leftPuv11, rightPuv10));
        // r4 = glm::length(leftPv11.coords) / glm::length(sumPv1);

        float t = 0.5 * (r1 + r2);
        std::cout << "r1: " << r1 << "\n";
        std::cout << "r2: " << r2 << "\n";
        std::cout << "t: " << t << "\n";
        return t;
    }

}