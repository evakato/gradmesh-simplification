#include <string>
#include <vector>
#include <omp.h>
#include <queue>
#include <set>

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
    void mergeTPRs();

private:
    std::vector<RegionAttributes> findMaxProductRegion(EdgeRegion &edgeRegion);
    std::vector<RegionAttributes> mergeRow(int currEdgeIdx, AABB &aabb, bool isRow = true, int maxLength = std::numeric_limits<int>::max(), int oppLength = 0);
    int mergeRowWithoutError(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    void mergeEdgeRegion(const Region &region);
    std::vector<EdgeRegion> getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs);
    void findAllRegions(const std::vector<int> &rowIdxs, int rowLength, AABB &errorAABB, std::vector<RegionAttributes> &regionAttributes);
    void createConflictGraph(std::vector<TPRNode> &allTPRs, float eps);
    void createAdjList();

    GradMeshMerger &merger;
    GmsAppState &appState;
    GradMesh &mesh;

    int productRegionIteration = 0;
    int productRegionIdx = 0;
    std::vector<EdgeRegion> sortedRegions;
    int totalRegionsIdx = 0;
    std::vector<std::pair<int, int>> meshCornerFaces;
    float specialThreshold;

    std::vector<std::vector<int>> adjList;
    std::vector<TPRNode> allTPRs;
};
