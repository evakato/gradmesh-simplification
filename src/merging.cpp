#include "gradmesh.hpp"

namespace Merging
{
    bool merge(GradMesh &mesh, std::vector<DoubleHalfEdge> &candidateMerges, int edgeId)
    {
        if (candidateMerges.size() > 0 && edgeId != -1)
        {
            int selectedHalfEdgeIdx = candidateMerges[edgeId].halfEdgeIdx1;
            mergePatches(mesh, selectedHalfEdgeIdx);
            candidateMerges = candidateEdges(mesh);
            return true;
        }
        return false;
    }

    int chooseEdge(const std::vector<DoubleHalfEdge> &candidateEdgeList)
    {
        if (candidateEdgeList.size() > 0)
            return candidateEdgeList[0].halfEdgeIdx1;
        return -1;
    }

    std::vector<DoubleHalfEdge> candidateEdges(const GradMesh &mesh)
    {
        std::vector<DoubleHalfEdge> edgeList = {};
        int faceIdx = 0;
        for (auto &face : mesh.getFaces())
        {
            if (face.halfEdgeIdx != -1)
            {
                int currIdx = face.halfEdgeIdx;
                for (int i = 0; i < 4; i++)
                {
                    const auto &currEdge = mesh.getEdge(currIdx);
                    if (currEdge.twinIdx != -1 && !currEdge.isBar() && !mesh.halfEdgeIsParent(currEdge.twinIdx))
                    {
                        auto findEdgeIdx = std::find_if(edgeList.begin(), edgeList.end(), [currIdx](const DoubleHalfEdge &dhe)
                                                        { return dhe.halfEdgeIdx2 == currIdx; });

                        if (findEdgeIdx == edgeList.end())
                        {
                            edgeList.push_back(DoubleHalfEdge{currIdx, currEdge.twinIdx, CurveId{faceIdx, i}});
                        }
                        else
                        {
                            size_t foundIndex = std::distance(edgeList.begin(), findEdgeIdx);
                            edgeList[foundIndex].curveId2 = CurveId{faceIdx, i};
                        }
                    }
                    currIdx = currEdge.nextIdx;
                }
                faceIdx++;
            }
        }
        return edgeList;
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

    void handleTJunction(GradMesh &mesh, const HalfEdge &edge1, const HalfEdge &edge2, int edge1Idx, float t)
    {
        std::cout << "tiwns: " << edge1.twinIdx << "< " << edge2.twinIdx << std::endl;
        if (edge1.twinIdx != -1 && edge2.twinIdx != -1)
        {
            std::cout << "adding a t-junction\n";
            int stemChildIdx = mesh.getEdge(edge2.twinIdx).nextIdx;
            std::cout << "next: " << stemChildIdx << "\n";

            if (mesh.getEdge(edge2.twinIdx).isStem())
            {
                mesh.addTJunction(edge2.twinIdx, edge1.twinIdx, stemChildIdx, edge1Idx, t, true);
            }
            else
            {
                mesh.addTJunction(edge2.twinIdx, edge1.twinIdx, stemChildIdx, edge1Idx, t, false);
            }
        }
    }

    void mergePatches(GradMesh &mesh, int mergeEdgeIdx)
    {
        int face1e1Idx = mergeEdgeIdx;
        auto &face1e1 = mesh.edges[face1e1Idx];
        auto &face2e1 = mesh.edges[face1e1.twinIdx];
        int face1e2Idx = face1e1.nextIdx;
        auto &face1e2 = mesh.edges[face1e2Idx];
        auto &face1e3 = mesh.edges[face1e2.nextIdx];
        int face1e4Idx = face1e3.nextIdx;
        auto &face1e4 = mesh.edges[face1e4Idx];
        int face2e2Idx = face2e1.nextIdx;
        auto &face2e2 = mesh.edges[face2e2Idx];
        int face2e3Idx = face2e2.nextIdx;
        auto &face2e3 = mesh.edges[face2e3Idx];
        auto &face2e4 = mesh.edges[face2e3.nextIdx];

        const float t1 = splittingFactor(mesh, face1e1, face1e2, face2e2, 1);
        const float t2 = splittingFactor(mesh, face2e1, face1e2, face2e4, 1);
        const float t = 0.5 * (t1 + t2);
        std::cout << "final t: " << t << std::endl;

        if (face1e1.isNormal())
        {
            std::cout << "Creating a merged patch at a normal half-edge f1e1\n";
            mesh.disablePoint(face1e1.originIdx);
            if (face2e2.isBar())
            {
                std::cout << "face2e2 is a bar.\n";
            }

            if (face2e3.isBar())
            {
                std::cout << "face2e3 is a bar.\n";
                face1e1.copyAll(face2e3);
            }
            else
            {
                face1e1.copyGeometricData(face2e3);
            }

            if (face1e4.isBar())
            {
                std::cout << "face1e4 is a bar.\n";
                auto &parent = mesh.edges[face1e4.parentIdx];
                auto &e1h1 = mesh.handles[parent.handleIdxs.first];
                auto &e1h2 = mesh.handles[parent.handleIdxs.second];
                auto &e2h2 = mesh.handles[face2e2.handleIdxs.second];
                float rescale = 1.0f / (1.0f + t);
                scaleHandles(e1h1, e1h2, e2h2, rescale);
                mesh.rescaleChildren(parent, rescale);
                handleTJunction(mesh, face1e4, face2e2, face1e4.parentIdx, 1 - rescale);
            }
            else
            {
                auto &e1h1 = mesh.handles[face1e4.handleIdxs.first];
                auto &e1h2 = mesh.handles[face1e4.handleIdxs.second];
                auto &e2h2 = mesh.handles[face2e2.handleIdxs.second];
                scaleHandles(e1h1, e1h2, e2h2, t);
                // TODO: make this work for when red is a bar (e.g. take this out of the else)
                handleTJunction(mesh, face1e4, face2e2, face1e4Idx, t);
            }
        }
        else if (face1e1.isStem())
        {
            std::cout << "Creating a merged patch at a stem half-edge f1e1\n";
            if (removeWholeTJunction(face1e4, face2e2))
            {
                std::cout << "Removing the whole T-junction" << std::endl;
                mesh.replaceChildWithParent(face1e4Idx);
                mesh.disablePoint(face1e1.originIdx);
                if (face2e3.isBar())
                {
                    std::cout << "face2e3 is a bar.\n";
                    face1e1.copyAll(face2e3);
                }
                else
                {
                    face1e1.copyGeometricData(face2e3);
                    face1e1.resetToNormal();
                }
            }
            else
            {
                face1e1.copyAll(face2e3);
            }
        }

        if (face2e1.isNormal())
        {
            std::cout << "Creating a merged patch at a normal half-edge f2e1\n";

            if (face1e3.isBar())
            {
                std::cout << "face1e3 is a bar.\n";
            }
            if (face2e4.isBar())
            {
                std::cout << "face2e4 is a bar.\n";
            }

            if (face1e2.isBar() && face1e3.isStem())
            {
                std::cout << "face1e2 is a bar and face1e3 is a stem :(\n";
                mesh.edges[face1e2.parentIdx].copyGeometricDataExceptHandles(face2e4);
                auto &parent = mesh.edges[face1e2.parentIdx];
                auto &e1h2 = mesh.handles[parent.handleIdxs.second];
                auto &e1h1 = mesh.handles[parent.handleIdxs.first];
                auto &e2h1 = mesh.handles[face2e4.handleIdxs.first];
                float rescale = t / (1.0f + t) + 1.0f;
                float rescale2 = 1.0f / (1.0f + t);

                float leftMul = 1.0f / rescale2;
                float rightMul = 1.0f / (1.0f - rescale2);
                e1h2 *= leftMul;
                e1h1 = e2h1 * rightMul;
                mesh.rescaleChildren(parent, rescale);
                handleTJunction(mesh, face2e4, face1e2, face1e2.parentIdx, rescale2);
            }
            else
            {
                if (face2e4.isStem())
                {
                    // realistically this shouldnt be an else if because the previous two cases aren't mutually exclusive..
                    std::cout << "face2e4 is a stem.\n";
                    face1e2.copyAll(face2e4);
                }
                else
                {
                    face1e2.copyGeometricData(face2e4);
                }
                auto &face1e2h1 = mesh.handles[face1e2.handleIdxs.first];
                auto &face1e2h2 = mesh.handles[face1e2.handleIdxs.second];
                auto &face2e4h1 = mesh.handles[face2e4.handleIdxs.first];
                scaleHandles(face1e2h2, face1e2h1, face2e4h1, t);
                float leftMul = 1.0f / t;
                float rightMul = 1.0f / (1.0f - t);
                face1e2h2 *= leftMul;
                face1e2h1 = face2e4h1 * rightMul;
                handleTJunction(mesh, face2e4, face1e2, face1e2Idx, t);
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

        mesh.copyEdgeTwin(face1e1Idx, face2e3Idx);
        std::cout << "patch removing: " << face2e1.patchIdx << std::endl;
        mesh.faces[face2e1.patchIdx].halfEdgeIdx = -1;
        face2e1.patchIdx = -1;
        face2e2.patchIdx = -1;
        face2e3.patchIdx = -1;
        face2e4.patchIdx = -1;
    }

    const float splittingFactor(GradMesh &mesh, HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign)
    {
        const auto &leftPv01 = mesh.getTangent(bar1, 1).coords;  // left patch: P_v(0,1)
        const auto &rightPv00 = mesh.getTangent(bar2, 0).coords; // right patch: P_v(0,0)
        const auto &leftPuv01 = stem.twist.coords;               // left patch: P_uv(0,1)
        const auto &rightPuv00 = bar2.twist.coords;              // right patch: P_uv(0,0)

        auto sumPv0 = absSum(leftPv01, rightPv00); // P_v(0,1) + P_v(0,0)

        float r1 = glm::length(leftPv01) / glm::length(sumPv0);
        float r2 = glm::length(absSum(leftPv01, sign * BCM * leftPuv01)) / glm::length((sumPv0 + sign * BCM * absSum(leftPuv01, rightPuv00)));

        float t = 0.5 * (r1 + r2);
        std::cout << "r1: " << r1 << " r2: " << r2 << " t: " << t << "\n";
        return t;
    }

}