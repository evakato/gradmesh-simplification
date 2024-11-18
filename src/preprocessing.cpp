#include "preprocessing.hpp"

void MergePreprocessor::preprocessSingleMergeError()
{
    if (appState.preprocessSingleMergeProgress % 100 == 0 &&
        appState.preprocessSingleMergeProgress != 0)
    {
        int start = appState.preprocessSingleMergeProgress - 100;
#pragma omp parallel for
        for (int i = start; i < appState.preprocessSingleMergeProgress; ++i)
        {
            auto &dhe = appState.candidateMerges[i];
            std::string imgPath = "preprocessing/e" + std::to_string(i) + ".png";
            dhe.error = merger.metrics.getMergeError(imgPath.c_str());
        }
        createDir(PREPROCESSING_DIR);
    }

    if (appState.preprocessSingleMergeProgress >= appState.candidateMerges.size())
    {
        int start = appState.preprocessSingleMergeProgress / 100 * 100;
#pragma omp parallel for
        for (int i = start; i < appState.candidateMerges.size(); ++i)
        {
            auto &dhe = appState.candidateMerges[i];
            std::string imgPath = "preprocessing/e" + std::to_string(i) + ".png";
            dhe.error = merger.metrics.getMergeError(imgPath.c_str());
        }
        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessSingleMergeProgress = -2;
        merger.metrics.setEdgeErrorMap(appState.candidateMerges);
        return;
    }

    if (appState.preprocessSingleMergeProgress == 0)
    {
        std::vector<CurveId> boundaryEdges;
        merger.select.findCandidateMerges(&boundaryEdges);
        merger.metrics.setBoundaryEdges(boundaryEdges);
        merger.metrics.captureBeforeMerge(appState.originalGlPatches);
    }

    auto &dhe = appState.candidateMerges[appState.preprocessSingleMergeProgress];
    int selectedHalfEdgeIdx = dhe.halfEdgeIdx1;

    merger.mergePatches(selectedHalfEdgeIdx);
    auto glPatches = getAllPatchGLData(
        mesh.generatePatches().value(),
        &Patch::getControlMatrix);
    std::string imgPath = "preprocessing/e" +
                          std::to_string(appState.preprocessSingleMergeProgress) + ".png";
    merger.metrics.captureAfterMerge(glPatches, imgPath.c_str());
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    appState.preprocessSingleMergeProgress++;
}

int MergePreprocessor::mergeRow(int currEdgeIdx, int maxLength)
{
    int length = 0;
    for (length; length < maxLength; length++)
    {
        auto &currEdge = mesh.edges[currEdgeIdx];
        if (!mesh.validEdgeType(currEdge))
            break;

        std::string imgPath = "preprocessing/e" + std::to_string(productRegionIdx) + ".png";
        if (!merger.merge(currEdgeIdx, imgPath))
            break;

        writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
    }
    if (length > 0)
        mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
    return length;
}

int MergePreprocessor::mergeRowWithoutError(int currEdgeIdx, int maxLength)
{
    int length = 0;
    for (length; length < maxLength; length++)
    {
        auto &currEdge = mesh.edges[currEdgeIdx];
        if (!mesh.validEdgeType(currEdge))
            break;

        merger.mergePatches(currEdgeIdx);
    }
    return length;
}

void MergePreprocessor::findMaxProductRegion(EdgeRegion &edgeRegion)
{
    auto [rowIdx, colIdx] = edgeRegion.gridPair;
    edgeRegion.faceIdx = mesh.edges[rowIdx].faceIdx;

    int rowLength = mergeRow(rowIdx);
    int rowChainLength = mesh.maxDependencyChain();
    mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");

    int colLength = mergeRow(colIdx);
    int colChainLength = mesh.maxDependencyChain();
    mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");

    int maxPatches = std::max(rowLength, colLength);
    std::pair<int, int> maxProductRegion;
    int finalChain = 0;

    if (rowLength >= colLength)
    {
        maxProductRegion = {rowLength, 0};
        finalChain = rowChainLength;
    }
    else
    {
        maxProductRegion = {0, colLength};
        finalChain = colChainLength;
    }

    if (rowLength == 0 || colLength == 0)
    {
        edgeRegion.maxChainLength = finalChain;
        edgeRegion.maxRegion = maxProductRegion;
        return;
    }

    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    for (int i = 0; i < colLength; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        rowIdxs.push_back(currRowIdx);
    }

    for (int i = 1; i < rowIdxs.size(); i++)
    {
        int currEdgeIdx = rowIdxs[i];
        int secondRow = mergeRow(currEdgeIdx, rowLength);
        for (int j = 0; j < i; j++)
        {
            mergeRowWithoutError(rowIdxs[j], secondRow);
        }

        int colsMerged = mergeRow(mesh.edges[rowIdxs[0]].nextIdx, i);
        int chainLength = mesh.maxDependencyChain();
        mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");
        if (colsMerged < i)
            break;

        rowLength = std::min(secondRow, rowLength);
        int numPatches = (rowLength + 1) * (i + 1);
        if (maxPatches < numPatches)
        {
            maxPatches = numPatches;
            maxProductRegion = {rowLength, i};
            finalChain = chainLength;
        }
    }

    std::cout << "Chain length: " << finalChain << std::endl;
    std::cout << "Max patches: " << maxPatches << std::endl;
    std::cout << "Max region: " << maxProductRegion.first << ", " << maxProductRegion.second << std::endl;

    edgeRegion.maxChainLength = finalChain;
    edgeRegion.maxRegion = maxProductRegion;
    return;
}

void MergePreprocessor::preprocessProductRegions()
{
    auto &edgeRegions = appState.edgeRegions;

    if (productRegionIdx == 0 && edgeRegions.empty())
        edgeRegions = getEdgeRegions(mesh);

    auto &currEdgeRegion = edgeRegions[productRegionIdx++];
    findMaxProductRegion(currEdgeRegion);
    // currEdgeRegion.faceIdx = mesh.edges[gridPair.first].faceIdx;
    mesh.getMaxProductRegion(currEdgeRegion);

    appState.preprocessProductRegionsProgress = static_cast<float>(productRegionIdx) / edgeRegions.size();
    if (productRegionIdx >= edgeRegions.size())
    {
        // merger.select.preprocess();
        sortedRegions = edgeRegions;
        std::ranges::sort(sortedRegions, [](const EdgeRegion &a, const EdgeRegion &b)
                          {
                              if (a.getMaxPatches() != b.getMaxPatches())
                              {
                                  return a.getMaxPatches() > b.getMaxPatches(); // Primary sort: descending
                              }
                              return a.maxChainLength < b.maxChainLength; // Tie-breaker: ascending
                          });

        for (const auto &region : sortedRegions)
        {
            if (region.getMaxPatches() > 1)
            {
                std::cout << "Grid Pair: (" << region.gridPair.first << ", " << region.gridPair.second << ")\n";
                std::cout << "Max Region: (" << region.maxRegion.first << ", " << region.maxRegion.second << ")\n";
                std::cout << "Face Index: " << region.faceIdx << '\n';
                std::cout << "Max Chain Length: " << region.maxChainLength << '\n';
            }
        }

        mergeEdgeRegion(sortedRegions[0]);
        std::erase_if(sortedRegions, [&](const auto &region)
                      {
                          const auto &edge = mesh.edges[region.gridPair.first];
                          return !edge.isValid(); });

        std::cout << sortedRegions.size() << std::endl;
        appState.updateMeshRender();
        writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);

        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessProductRegionsProgress = -1.0f;

        edgeRegions = sortedRegions;
        productRegionIdx = 0;
    }
}

void MergePreprocessor::mergeEdgeRegion(const EdgeRegion &edgeRegion)
{
    int rowIdx = edgeRegion.gridPair.first;
    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    for (int i = 0; i < edgeRegion.maxRegion.second; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        rowIdxs.push_back(currRowIdx);
    }

    for (int i = 0; i < rowIdxs.size(); i++)
    {
        mergeRowWithoutError(rowIdxs[i], edgeRegion.maxRegion.first);
    }
    mergeRowWithoutError(mesh.edges[rowIdxs[0]].nextIdx, edgeRegion.maxRegion.second);
}

std::vector<EdgeRegion> getEdgeRegions(const GradMesh &mesh)
{
    std::vector<EdgeRegion> edgeRegions;
    auto cornerFaces = mesh.findCornerFace();
    auto gridIdxs = mesh.getGridEdgeIdxs(cornerFaces[0].first, cornerFaces[0].second);
    constexpr auto makeEdgeRegion = std::views::transform([](std::pair<int, int> gridIdxs)
                                                          { return EdgeRegion{gridIdxs, {0, 0}}; });
    std::ranges::copy(gridIdxs | makeEdgeRegion, std::back_inserter(edgeRegions));

    return edgeRegions;
}
