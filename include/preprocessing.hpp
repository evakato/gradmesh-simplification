#include <string>
#include <vector>
#include <omp.h>

#include "gms_appstate.hpp"
#include "gradmesh.hpp"
#include "merging.hpp"

class MergePreprocessor
{
public:
    MergePreprocessor(GradMeshMerger &merger, GmsAppState &appState) : merger(merger), appState(appState), mesh(appState.mesh) {}
    void preprocessSingleMergeError();
    void preprocessProductRegions();

private:
    void findMaxProductRegion(EdgeRegion &edgeRegion);
    int mergeRow(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    int mergeRowWithoutError(int currEdgeIdx, int maxLength = std::numeric_limits<int>::max());
    void mergeEdgeRegion(const EdgeRegion &edgeRegion);

    GradMeshMerger &merger;
    GmsAppState &appState;
    GradMesh &mesh;

    int productRegionIteration = 0;
    int productRegionIdx = 0;
    std::vector<EdgeRegion> sortedRegions;
};

std::vector<EdgeRegion> getEdgeRegions(const GradMesh &mesh);