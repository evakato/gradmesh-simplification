#include "gradmesh.hpp"

namespace Merging
{
    void merge(GradMesh &mesh)
    {
        mesh.candidateMerges();
        int selectedEdge = chooseEdge(mesh.faces, mesh.edges);
        std::cout << "selected edge: " << selectedEdge << std::endl;
        if (selectedEdge != -1)
            mergePatches(mesh, selectedEdge);
        else
            std::cout << "merging not possible\n";
    }

    int chooseEdge(const std::vector<Face> &faces, const std::vector<HalfEdge> &edges)
    {
        for (auto &face : faces)
        {
            if (face.halfEdgeIdx != -1)
            {
                int currIdx = face.halfEdgeIdx;
                for (int i = 0; i < 4; i++)
                {
                    auto &currEdge = edges[currIdx];
                    if (currEdge.twinIdx != -1 && (currEdge.parentIdx == -1 || currEdge.interval.x == currEdge.interval.y) && !currEdge.twinIsTJunction)
                        return currIdx;
                    currIdx = currEdge.nextIdx;
                }
            }
        }
        return -1;
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

        if (face1e1.isStem() && face2e1.isStem())
        {
            std::cout << "What!" << std::endl;
            face1e4.interval.x = std::min(face1e4.interval.x, face2e2.interval.x);
            face1e4.interval.y = std::max(face1e4.interval.y, face2e2.interval.y);
            face1e2.interval.x = std::min(face1e2.interval.x, face2e4.interval.x);
            face1e2.interval.y = std::max(face1e2.interval.y, face2e4.interval.y);
            if (face1e4.interval.x == 0 && face1e4.interval.y == 1 && face1e2.interval.x == 0 && face1e2.interval.y == 1)
            {
                // face1e1.parentIdx = -1;
                mesh.replaceChildWithParent(face1e2Idx);
                mesh.replaceChildWithParent(face1e4Idx);
                face1e1.originIdx = face2e3.originIdx;
                face1e1.color = face2e3.color;
                face1e1.parentIdx = -1;
                face1e1.handleIdxs = face2e3.handleIdxs;
            }
            else
            {
                face1e1.interval = face2e3.interval;
                face1e1.handleIdxs = face2e3.handleIdxs;
                face1e1.color = face2e3.color;
                face1e2.color = face2e4.color;
                face1e1.parentIdx = face2e3.parentIdx;
            }
            mesh.faces[face2e1.patchIdx].halfEdgeIdx = -1;

            face1e1.twinIdx = face2e3.twinIdx;
            if (face1e1.twinIdx != -1)
                mesh.edges[face1e1.twinIdx].twinIdx = mergeEdgeIdx;

            face2e1.patchIdx = -1;
            face2e2.patchIdx = -1;
            face2e3.patchIdx = -1;
            face2e4.patchIdx = -1;
            std::cout << mesh;
            return;
        }

        bool tailBarIsT = twoChildrenSameParent(face1e4.parentIdx, face2e2.parentIdx);
        bool headBarIsT = twoChildrenSameParent(face1e2.parentIdx, face2e4.parentIdx);

        std::pair<int, int> childrenMerge = {0, 0};

        if (!tailBarIsT)
        {
            mesh.points[face1e1.originIdx].halfEdgeIdx = -1;
            face1e1.originIdx = face2e3.originIdx;
            face1e1.color = face2e3.color;
        }
        else
        {
            mesh.replaceChildWithParent(face1e4Idx);
            childrenMerge.first = 1;
        }
        if (!headBarIsT)
        {
            mesh.points[face1e2.originIdx].halfEdgeIdx = -1;
            face1e2.originIdx = face2e4.originIdx;
            face1e2.color = face2e4.color;
        }
        else
        {
            mesh.replaceChildWithParent(face1e2Idx);
            childrenMerge.second = 1;
        }

        const float t = splittingFactor(mesh, face1e1, face1e2, face1e4, face2e1, face2e2, face2e4, childrenMerge);
        float leftMul = 1.0f / t;
        float rightMul = 1.0f / (1.0f - t);

        auto &face1e2h2 = mesh.handles[face1e2.handleIdxs.second];
        auto &face1e4h1 = mesh.handles[face1e4.handleIdxs.first];

        auto &face1e2h1 = mesh.handles[face1e2.handleIdxs.first];
        auto &face1e4h2 = mesh.handles[face1e4.handleIdxs.second];
        auto &face2e2h2 = mesh.handles[face2e2.handleIdxs.second];
        auto &face2e4h1 = mesh.handles[face2e4.handleIdxs.first];

        if (face1e4.parentIdx == -1 && face2e2.parentIdx == -1)
        {
            face1e4h1 *= leftMul;
            face1e4h2 = face2e2h2 * rightMul;
        }
        if (face1e2.parentIdx == -1 && face2e4.parentIdx == -1)
        {
            face1e2h1 = face2e4h1 * rightMul;
            face1e2h2 *= leftMul;
        }

        face1e1.twinIdx = face2e3.twinIdx;
        if (face2e3.twinIdx != -1)
            mesh.edges[face2e3.twinIdx].twinIdx = mergeEdgeIdx;

        // Adding a t-junction: check that these edges have twins
        if (face1e2.twinIdx != -1 && face2e4.twinIdx != -1)
        {
            int stemChild = mesh.edges[face1e2.twinIdx].nextIdx;
            std::cout << "inside first t junction " << face1e2.twinIdx << ", " << face2e4.twinIdx << ", " << std::endl;
            mesh.addTJunction(face1e2.twinIdx, face2e4.twinIdx, stemChild, face1e2Idx, t);
        }
        if (face1e4.twinIdx != -1 && face2e2.twinIdx != -1)
        {
            int stemChild = mesh.edges[face1e4.twinIdx].nextIdx;
            mesh.addTJunction(face1e4.twinIdx, face2e2.twinIdx, stemChild, face1e2Idx, t);
        }

        std::cout << "patch removing: " << face2e1.patchIdx << std::endl;
        mesh.faces[face2e1.patchIdx].halfEdgeIdx = -1;
        face2e1.patchIdx = -1;
        face2e2.patchIdx = -1;
        face2e3.patchIdx = -1;
        face2e4.patchIdx = -1;
        std::cout << mesh;
    }

    const float splittingFactor(GradMesh &mesh, HalfEdge &face1e1, HalfEdge &face1e2, HalfEdge &face1e4, HalfEdge &face2e1, HalfEdge &face2e2, HalfEdge &face2e4, std::pair<int, int> childrenMerge)
    {
        // im only using the geometric data rn, idk if i should include color
        const auto &leftPv01 = mesh.getTangent(face1e4, 1);  // left patch: P_v(0,1)
        const auto &leftPv11 = mesh.getTangent(face1e2, 0);  // left patch: P_v(1,1)
        const auto &rightPv00 = mesh.getTangent(face2e2, 0); // right patch: P_v(0,0)
        const auto &rightPv10 = mesh.getTangent(face2e4, 1); // right patch: P_v(1,0)
        const glm::vec2 &leftPuv01 = face1e1.twist.coords;   // left patch: P_uv(0,1)
        const glm::vec2 &leftPuv11 = face1e2.twist.coords;   // left patch: P_uv(1,1)
        const glm::vec2 &rightPuv00 = face2e2.twist.coords;  // right patch: P_uv(0,0)
        const glm::vec2 &rightPuv10 = face2e1.twist.coords;  // right patch: P_uv(1,0)

        glm::vec2 sumPv0 = absSum(leftPv01.coords, rightPv00.coords); // P_v(0,1) + P_v(0,0)
        glm::vec2 sumPv1 = absSum(leftPv11.coords, rightPv10.coords); // P_v(1,1) + P_v(1,0)

        float r1 = glm::length(leftPv01.coords) / glm::length(sumPv0);
        float r2 = glm::length(absSum(leftPv01.coords, -BCM * leftPuv01)) / glm::length(sumPv0 - BCM * absSum(leftPuv01, rightPuv00));
        float r3 = 0;
        float r4 = 0;
        float avgFactor = 0.5;
        if (!childrenMerge.second)
        {
            r3 = glm::length(absSum(leftPv11.coords, BCM * leftPuv11)) / glm::length(sumPv1 + BCM * absSum(leftPuv11, rightPuv10));
            r4 = glm::length(leftPv11.coords) / glm::length(sumPv1);
            avgFactor = 0.25;
        }

        float t = avgFactor * (r1 + r2 + r3 + r4);
        std::cout << "r1: " << r1 << "\n";
        std::cout << "r2: " << r2 << "\n";
        std::cout << "r3: " << r3 << "\n";
        std::cout << "r4: " << r4 << "\n";
        std::cout << "t: " << t << "\n";
        return t;
    }

}