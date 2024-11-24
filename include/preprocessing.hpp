#include <string>
#include <vector>
#include <omp.h>

#include "gms_appstate.hpp"
#include "gradmesh.hpp"
#include "merging.hpp"

class MergePreprocessor
{
public:
    MergePreprocessor(GradMeshMerger &merger, GmsAppState &appState) : merger(merger), appState(appState), mesh(appState.mesh)
    {
    }
    void preprocessSingleMergeError();
    void preprocessProductRegions();
    void setEdgeRegions();

private:
    void findMaxProductRegion(EdgeRegion &edgeRegion);
    int mergeRow(int currEdgeIdx, AABB &aabb, int maxLength = std::numeric_limits<int>::max());
    int mergeRowWithoutError(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    void mergeEdgeRegion(const EdgeRegion &edgeRegion);
    std::vector<EdgeRegion> getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs);

    GradMeshMerger &merger;
    GmsAppState &appState;
    GradMesh &mesh;

    int productRegionIteration = 0;
    int productRegionIdx = 0;
    std::vector<EdgeRegion> sortedRegions;
    int totalRegionsIdx = 0;
    std::vector<std::pair<int, int>> meshCornerFaces;
    float specialThreshold;
};
