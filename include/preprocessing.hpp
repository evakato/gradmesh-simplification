#include <string>
#include <vector>
#include <omp.h>
#include <queue>
#include <set>
#include <iterator>

#include "gms_appstate.hpp"
#include "gradmesh.hpp"
#include "merging.hpp"

class MergePreprocessor
{
    struct TPRNodePair
    {
        int i;
        int j;
        float score;
    };

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
    void mergeIndependentSet();
    void mergeMotorcycle();

private:
    std::vector<RegionAttributes> findMaxProductRegion(EdgeRegion &edgeRegion);
    std::vector<RegionAttributes> mergeRow(int currEdgeIdx, AABB &aabb, bool isRow = true, int maxLength = std::numeric_limits<int>::max(), int oppLength = 0);
    int mergeRowWithoutError(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    void mergeEdgeRegion(const Region &region);
    void mergeEdgeRegionWithError(const Region &region);
    std::vector<EdgeRegion> getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs);
    void findAllRegions(const std::vector<int> &rowIdxs, int rowLength, AABB &errorAABB, std::vector<RegionAttributes> &regionAttributes);
    void createAdjList();
    bool canInclude(int idx);
    int binarySearch(const std::vector<int> &arr, int left, float eps);

    void greedyQuadErrorHeuristic(float eps);
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
    std::set<int> currIndependentSet;
    std::set<int>::iterator currIndependentSetIterator;
};
