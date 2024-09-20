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
                    if (currEdge.twinIdx != -1 && currEdge.parentIdx == -1)
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
        std::cout << "twin is: " << face1e1.twinIdx << "\n";

        auto &face1e2 = mesh.edges[face1e1.nextIdx];
        auto &face1e3 = mesh.edges[face1e2.nextIdx];
        auto &face1e4 = mesh.edges[face1e3.nextIdx];

        auto &face2e2 = mesh.edges[face2e1.nextIdx];
        auto &face2e3 = mesh.edges[face2e2.nextIdx];
        auto &face2e4 = mesh.edges[face2e3.nextIdx];

        std::pair<int, int> childrenMerge = {0, 0};
        face1e1.originIdx = face2e3.originIdx;
        face1e1.color = face2e3.color;

        if (face1e2.parentIdx == -1 && face2e4.parentIdx == -1)
        {
            face1e2.originIdx = face2e4.originIdx;
            face1e2.color = face2e4.color;
        }
        else
        {
            auto &parent = mesh.edges[face1e2.parentIdx];
            face1e2.originIdx = parent.originIdx;
            face1e2.handleIdxs = parent.handleIdxs;
            face1e2.color = parent.color;
            face1e2.parentIdx = -1;
            childrenMerge.second = 1;
        }

        const float t = splittingFactor(mesh, face1e1, face1e2, face1e4, face2e1, face2e2, face2e4, childrenMerge);
        float leftMul = 1.0f / t;
        float rightMul = 1.0f / (1.0f - t);

        auto &face1e2h2 = mesh.handles[face1e2.handleIdxs.second];
        auto &face1e4h1 = mesh.handles[face1e4.handleIdxs.first];
        std::cout << "HANDLE TEST\n";
        std::cout << face1e2h2;
        std::cout << "\n";
        std::cout << face1e4h1;

        face1e2h2 *= leftMul;
        face1e4h1 *= leftMul;

        auto &face1e2h1 = mesh.handles[face1e2.handleIdxs.first];
        auto &face1e4h2 = mesh.handles[face1e4.handleIdxs.second];
        auto &face2e2h2 = mesh.handles[face2e2.handleIdxs.second];
        auto &face2e4h1 = mesh.handles[face2e4.handleIdxs.first];
        std::cout << "\n";
        std::cout << face2e4h1;
        std::cout << "\n";
        std::cout << face2e2h2;
        std::cout << "\n";
        face1e2h1 = face2e4h1 * rightMul;
        face1e4h2 = face2e2h2 * rightMul;

        std::cout << "HANDLE TEST2\n";
        std::cout << face1e2h2;
        std::cout << "\n";
        std::cout << face1e4h1;
        std::cout << "\n";
        std::cout << face1e2h1;
        std::cout << "\n";
        std::cout << face1e4h2;
        std::cout << "\n";

        face1e1.twinIdx = face2e3.twinIdx;
        if (face2e3.twinIdx != -1)
            mesh.edges[face2e3.twinIdx].twinIdx = mergeEdgeIdx;

        if (face1e2.twinIdx != -1 && face2e4.twinIdx != -1)
        {
            HalfEdge parentEdge;
            auto &twinChild1 = mesh.edges[face1e2.twinIdx];
            auto &twinChild2 = mesh.edges[face2e4.twinIdx];
            auto &stemChild = mesh.edges[twinChild1.nextIdx];

            // std::cout << "trick stem is:  " << twinChild1.nextIdx << "\n";

            parentEdge.color = twinChild1.color;
            parentEdge.handleIdxs = {face1e2.handleIdxs.second, face1e2.handleIdxs.first};
            parentEdge.twinIdx = face1e1.nextIdx;
            parentEdge.originIdx = twinChild1.originIdx;
            parentEdge.nextIdx = twinChild2.nextIdx;
            parentEdge.parentIdx = -1;
            int parentIdx = mesh.addEdge(parentEdge);

            twinChild1.parentIdx = parentIdx;
            twinChild2.parentIdx = parentIdx;
            stemChild.parentIdx = parentIdx;
            twinChild1.handleIdxs = {-1, -1};
            twinChild2.handleIdxs = {-1, -1};
            twinChild1.originIdx = -1;
            twinChild2.originIdx = -1;
            stemChild.originIdx = -1;
            twinChild1.interval = {0, t};
            twinChild2.interval = {t, 1};
            stemChild.interval = {t, t};

            twinChild1.twinIdx = -1;
            twinChild2.twinIdx = -1;
            face1e2.twinIdx = -1;
        }

        // auto &bottomEdge1Handle1 = mesh.handles[bottomEdge1.handleIdxs.first];
        // auto &bottomEdge1Handle2 = mesh.handles[bottomEdge1.handleIdxs.second];
        //  bottomEdge1Handle1 = bottomEdge1Handle1 * 2;
        //  bottomEdge1Handle2 = bottomEdge1Handle2 * 2;
        //   auto &bottomPoint2 = mesh.points[bottomEdge2.originIdx];
        //   auto &leftPoint2 = mesh.points[leftEdge2.originIdx];

        // mesh.faces.erase(mesh.faces.begin() + face2e1.patchIdx);
        mesh.faces[face2e1.patchIdx].halfEdgeIdx = -1;
        std::cout << "patch removing: " << face2e1.patchIdx << std::endl;
        // twinEdge.patchIdx = -1;
        // rightEdge2.patchIdx = -1;
        // bottomEdge2.patchIdx = -1;
        // leftEdge2.patchIdx = -1;
        // std::cout << mesh;
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

        std::cout << leftPv11 << " oke\n";
        std::cout << rightPv10 << " oke\n";

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