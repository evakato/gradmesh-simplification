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
    MergePreprocessor(GradMeshMerger &merger, GmsAppState &appState) : merger(merger), appState(appState), mesh(appState.mesh), edgeRegions(appState.edgeRegions)
    {
    }
    void preprocessSingleMergeError();
    void preprocessProductRegions();
    void loadProductRegionsPreprocessing();
    void setEdgeRegions();
    void mergeTPRs();
    void mergeGreedyQuadError();

private:
    std::vector<RegionAttributes> findMaxProductRegion(EdgeRegion &edgeRegion);
    std::vector<RegionAttributes> mergeRow(int currEdgeIdx, AABB &aabb, bool isRow = true, int maxLength = std::numeric_limits<int>::max(), int oppLength = 0);
    int mergeRowWithoutError(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    void mergeEdgeRegion(const Region &region);
    std::vector<EdgeRegion> getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs);
    void findAllRegions(const std::vector<int> &rowIdxs, int rowLength, AABB &errorAABB, std::vector<RegionAttributes> &regionAttributes);
    void createAdjList();
    void vertexColoring();
    int binarySearch(const std::vector<int> &arr, int left, float eps);

    void createConflictGraph(float eps);
    void findIndependentSetWithColoring(float eps);
    void heuristicBinarySearch(float userError);
    void greedyQuadErrorTwoStep(float userError);
    void computeConflictGraphStats();

    GradMeshMerger &merger;
    GmsAppState &appState;
    GradMesh &mesh;
    std::vector<EdgeRegion> &edgeRegions;

    int productRegionIdx = 0;
    std::vector<std::pair<int, int>> meshCornerFaces;

    std::vector<TPRNode> allTPRs;
    std::vector<std::vector<int>> adjList;
    std::vector<int> vertexColors;
};
