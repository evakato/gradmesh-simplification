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
            dhe.error = merger.metrics.evaluateMetric(imgPath.c_str());
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
            dhe.error = merger.metrics.evaluateMetric(imgPath.c_str());
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
        // merger.metrics.captureBeforeMerge(appState.originalGlPatches);
    }

    auto &dhe = appState.candidateMerges[appState.preprocessSingleMergeProgress];
    int selectedHalfEdgeIdx = dhe.halfEdgeIdx1;

    merger.mergePatches(selectedHalfEdgeIdx);
    auto glPatches = getAllPatchGLData(mesh.generatePatches().value(), &Patch::getControlMatrix);
    std::string imgPath = "preprocessing/e" + std::to_string(appState.preprocessSingleMergeProgress) + ".png";
    merger.metrics.captureAfterMerge(glPatches, imgPath.c_str());
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    appState.preprocessSingleMergeProgress++;
}

int MergePreprocessor::mergeRow(int currEdgeIdx, AABB &aabb, int maxLength)
{
    int length = 0;
    for (length; length < maxLength; length++)
    {
        auto &currEdge = mesh.edges[currEdgeIdx];
        if (!mesh.validMergeEdge(currEdge))
            break;

        aabb.expand(mesh.getAffectedMergeAABB(currEdgeIdx));
        aabb.constrain(appState.mergeSettings.globalAABB);
        aabb.ensureSize(MIN_AABB_SIZE);
        float mergeError = merger.attemptMerge(currEdgeIdx, aabb);

        if (appState.mergeSettings.pixelRegion == MergeMetrics::PixelRegion::Local)
            mergeError *= (aabb.area() / appState.mergeSettings.globalAABB.area());

        if (mergeError > specialThreshold)
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
        if (!mesh.validMergeEdge(currEdge))
            break;

        merger.mergePatches(currEdgeIdx);
    }
    return length;
}

void MergePreprocessor::findMaxProductRegion(EdgeRegion &edgeRegion)
{
    AABB errorAABB{};
    auto [rowIdx, colIdx] = edgeRegion.gridPair;

    int rowLength = mergeRow(rowIdx, errorAABB);
    int rowChainLength = mesh.maxDependencyChain();
    mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");

    int colLength = mergeRow(colIdx, errorAABB);
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
        if (currRowIdx == -1)
            break;
        rowIdxs.push_back(currRowIdx);
    }

    for (int i = 1; i < rowIdxs.size(); i++)
    {
        int currEdgeIdx = rowIdxs[i];
        int secondRow = mergeRow(currEdgeIdx, errorAABB, rowLength);
        for (int j = 0; j < i; j++)
        {
            mergeRowWithoutError(rowIdxs[j], secondRow);
        }

        int colsMerged = mergeRow(mesh.edges[rowIdxs[0]].nextIdx, errorAABB, i);
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

    edgeRegion.maxChainLength = finalChain;
    edgeRegion.maxRegion = maxProductRegion;
    return;
}

void MergePreprocessor::preprocessProductRegions()
{
    auto &edgeRegions = appState.edgeRegions;
    auto &currEdgeRegion = edgeRegions[productRegionIdx++];

    findMaxProductRegion(currEdgeRegion);
    mesh.getProductRegionAABB(currEdgeRegion);

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

        mergeEdgeRegion(sortedRegions[0]);
        sortedRegions.erase(sortedRegions.begin());
        std::erase_if(sortedRegions, [&](const auto &region)
                      {
                                  const auto &edge1 = mesh.edges[region.gridPair.first];
                                  const auto &edge2 = mesh.edges[region.gridPair.second];
                                  return (!mesh.validMergeEdge(edge1) && !mesh.validMergeEdge(edge2)); });
        std::erase_if(sortedRegions, [&](const auto &region)
                      { return region.getMaxPatches() == 1; });

        std::cout << "size " << sortedRegions.size() << std::endl;

        appState.updateMeshRender();
        writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);

        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessProductRegionsProgress = -1.0f;

        merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
        float err = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
        specialThreshold = 0.0005f + err;
        std::cout << "Total error: " << err << " new theshold " << specialThreshold << std::endl;

        edgeRegions = sortedRegions;
        if (edgeRegions.empty())
        {
            /*
            ++totalRegionsIdx;
            if (totalRegionsIdx < meshCornerFaces.size())
                edgeRegions = getEdgeRegions(appState.mesh, meshCornerFaces[totalRegionsIdx]);
                */
        }
        productRegionIdx = 0;
    }
}

void MergePreprocessor::mergeEdgeRegion(const EdgeRegion &edgeRegion)
{
    int rowIdx = edgeRegion.gridPair.first;
    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    if (edgeRegion.maxRegion.first == 0)
    {
        mergeRowWithoutError(edgeRegion.gridPair.second, edgeRegion.maxRegion.second);
        return;
    }
    for (int i = 0; i < edgeRegion.maxRegion.second; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        if (currRowIdx == -1)
            break;
        rowIdxs.push_back(currRowIdx);
    }

    for (int i = 0; i < rowIdxs.size(); i++)
    {
        mergeRowWithoutError(rowIdxs[i], edgeRegion.maxRegion.first);
    }
    mergeRowWithoutError(mesh.edges[rowIdxs[0]].nextIdx, edgeRegion.maxRegion.second);
}

void MergePreprocessor::setEdgeRegions()
{
    meshCornerFaces = mesh.findCornerFaces();
    // totalRegionsIdx = 2;
    specialThreshold = appState.mergeSettings.errorThreshold;
    // assert(meshCornerFaces.size() > totalRegionsIdx);
    appState.edgeRegions = getEdgeRegions(meshCornerFaces);
}

std::vector<EdgeRegion> MergePreprocessor::getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs)
{
    std::vector<EdgeRegion> edgeRegions;
    for (auto &startPair : startPairs)
    {
        if (startPair.first < 0 || startPair.first >= mesh.getEdges().size())
        {
            std::cout << "Invalid start pair index: " << startPair.first << std::endl;
            continue;
        }
        int faceIdx = mesh.edges[startPair.first].faceIdx;
        auto it = std::find_if(edgeRegions.begin(), edgeRegions.end(),
                               [faceIdx](const EdgeRegion &region)
                               {
                                   return region.faceIdx == faceIdx;
                               });
        if (it != edgeRegions.end())
            continue;

        auto gridIdxs = mesh.getGridEdgeIdxs(startPair.first, startPair.second);
        for (auto &gridIdxPair : gridIdxs)
        {
            if (gridIdxPair.first < 0 || gridIdxPair.first >= mesh.getEdges().size())
            {
                std::cout << "Invalid grid edge index: " << gridIdxPair.first << std::endl;
                continue;
            }
            int faceIdx = mesh.edges[gridIdxPair.first].faceIdx;
            edgeRegions.push_back(EdgeRegion{gridIdxPair, {0, 0}, faceIdx});
        }
    }
    std::cout << "size of edge regions" << edgeRegions.size() << std::endl;

    return edgeRegions;
}
