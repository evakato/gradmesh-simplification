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
    merger.metrics.captureGlobalImage(glPatches, imgPath.c_str());
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    appState.preprocessSingleMergeProgress++;
}

std::vector<RegionAttributes> MergePreprocessor::mergeRow(int currEdgeIdx, AABB &aabb, bool isRow, int maxLength, int oppLength)
{
    std::vector<RegionAttributes> mergeRowList;
    int length = 0;
    for (length; length < maxLength; length++)
    {
        auto &currEdge = mesh.edges[currEdgeIdx];
        if (!mesh.validMergeEdge(currEdge))
            break;

        if (appState.mergeSettings.pixelRegion == MergeMetrics::PixelRegion::Local)
        {
            aabb.expand(mesh.getAffectedMergeAABB(currEdgeIdx));
            aabb.constrain(appState.mergeSettings.globalAABB);
            aabb.ensureSize(MIN_AABB_SIZE);
        }

        float mergeError = merger.attemptMerge(currEdgeIdx, aabb);

        if (appState.mergeSettings.pixelRegion == MergeMetrics::PixelRegion::Local)
            mergeError *= (aabb.area() / appState.mergeSettings.globalAABB.area());

        if (mergeError > appState.mergeSettings.errorThreshold)
            break;

        std::pair<int, int> regionPair = isRow ? std::make_pair(length + 1, oppLength) : std::make_pair(oppLength, length + 1);

        mergeRowList.push_back({regionPair, mergeError, mesh.maxDependencyChain()});
        writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
    }
    if (length > 0)
        mesh = readHemeshFile("mesh_saves/lastsave.hemesh");

    return mergeRowList;
}

int MergePreprocessor::mergeRowWithoutError(int currEdgeIdx, int maxLength)
{
    int length = 0;
    for (length; length < maxLength; length++)
    {
        auto &currEdge = mesh.edges[currEdgeIdx];
        if (!mesh.validMergeEdge(currEdge))
            merger.mergePatches(currEdgeIdx);
    }
    return length;
}

std::vector<RegionAttributes> MergePreprocessor::findMaxProductRegion(EdgeRegion &edgeRegion)
{
    AABB errorAABB{};
    auto [rowIdx, colIdx] = edgeRegion.gridPair;
    std::vector<RegionAttributes> regionAttributes;

    auto mergedRows = mergeRow(rowIdx, errorAABB);
    std::ranges::copy(mergedRows, std::back_inserter(regionAttributes));
    int rowLength = mergedRows.empty() ? 0 : mergedRows.back().maxRegion.first;
    mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");

    auto mergedCols = mergeRow(colIdx, errorAABB, false);
    std::ranges::copy(mergedCols, std::back_inserter(regionAttributes));
    int colLength = mergedCols.empty() ? 0 : mergedCols.back().maxRegion.second;
    mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");

    if (rowLength == 0 || colLength == 0)
        return regionAttributes;

    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    for (int i = 0; i < colLength; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        if (currRowIdx == -1)
            break;
        rowIdxs.push_back(currRowIdx);
    }

    findAllRegions(rowIdxs, rowLength, errorAABB, regionAttributes);
    return regionAttributes;
}

void MergePreprocessor::findAllRegions(const std::vector<int> &rowIdxs, int rowLength, AABB &errorAABB, std::vector<RegionAttributes> &regionAttributes)
{
    int mergeColIdx = rowIdxs.empty() ? -1 : mesh.edges[rowIdxs[0]].nextIdx;
    for (int rowIdx = 1; rowIdx < rowIdxs.size(); rowIdx++)
    {
        int currEdgeIdx = rowIdxs[rowIdx];
        for (int rowCell = 1; rowCell <= rowLength; rowCell++)
        {
            auto secondRowMerges = mergeRow(currEdgeIdx, errorAABB, true, rowCell);
            if (secondRowMerges.size() < rowCell)
            {
                rowLength = rowCell;
                break;
            }

            for (int j = 0; j < rowIdx; j++)
                mergeRowWithoutError(rowIdxs[j], rowCell);

            auto colMerges = mergeRow(mergeColIdx, errorAABB, false, rowIdx, rowCell);
            mesh = readHemeshFile("mesh_saves/save_" + std::to_string(productRegionIteration) + ".hemesh");
            if (colMerges.size() < rowIdx)
            {
                // std::cout << "jammer again " << rowIdx << " " << colMerges.size() << std::endl;
                break;
            }

            auto lastColMerge = colMerges.back();
            regionAttributes.push_back(lastColMerge);
        }
    }
}

void MergePreprocessor::preprocessProductRegions()
{
    auto &edgeRegions = appState.edgeRegions;
    auto &currEdgeRegion = edgeRegions[productRegionIdx++];

    auto allRegions = findMaxProductRegion(currEdgeRegion);
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    if (!allRegions.empty())
    {
        for (auto &region : allRegions)
        {
            region.maxRegionAABB = mesh.getProductRegionAABB(currEdgeRegion.gridPair, region.maxRegion);
        }
        currEdgeRegion.setAndSortAttributes(allRegions);
    }

    appState.preprocessProductRegionsProgress = static_cast<float>(productRegionIdx) / edgeRegions.size();

    if (productRegionIdx >= edgeRegions.size())
    {
        createAdjList();
        vertexColoring();
        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessProductRegionsProgress = -2.0f;
        productRegionIdx = 0;
    }
}

void MergePreprocessor::createAdjList()
{
    sortedRegions = appState.edgeRegions;
    std::ranges::sort(sortedRegions, std::greater<>());
    for (auto &sortedRegion : sortedRegions)
    {
        std::ranges::copy(sortedRegion.generateAllTPRs(), std::back_inserter(allTPRs));
    }

    // resort by maxPatches
    std::sort(allTPRs.begin(), allTPRs.end(), [](const TPRNode &a, const TPRNode &b)
              { return a.getMaxPatches() > b.getMaxPatches(); });

    adjList.resize(allTPRs.size());
    for (int i = 0; i < allTPRs.size(); i++)
    {
        auto &currentTPR = allTPRs[i];
        for (int j = i + 1; j < allTPRs.size(); j++)
        {
            auto &compTPR = allTPRs[j];
            if (mesh.regionsOverlap({currentTPR.gridPair, currentTPR.maxRegion}, {compTPR.gridPair, compTPR.maxRegion}))
            {
                adjList[i].push_back(j);
                adjList[j].push_back(i);
            }
        }
    }
    int degreeSum = 0;
    for (int i = 0; i < allTPRs.size(); i++)
    {
        allTPRs[i].id = i;
        allTPRs[i].degree = adjList[i].size();
        degreeSum += allTPRs[i].degree;
    }

    appState.conflictGraphStats.numOfNodes = allTPRs.size();
    appState.conflictGraphStats.avgDegree = static_cast<float>(degreeSum) / allTPRs.size();
}

void MergePreprocessor::mergeTPRs()
{
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    // createConflictGraph(allTPRs, appState.totalRegionsError);
    findIndependentSetWithColoring(appState.totalRegionsError);
    appState.updateMeshRender();
    writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);

    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
    appState.mergeProcess = MergeProcess::Merging;
}

void MergePreprocessor::createConflictGraph(float eps)
{
    auto cmp = [](const TPRNode &a, const TPRNode &b)
    { return a < b; };
    std::priority_queue<TPRNode, std::vector<TPRNode>, decltype(cmp)> pq(cmp);
    for (const auto &tpr : allTPRs)
    {
        pq.push(tpr);
    }

    std::set<int> independentSet;
    float totalError = 0;
    while (!pq.empty() && totalError < eps)
    {
        auto current = pq.top();
        pq.pop();

        bool canInclude = true;
        for (int neighbor : adjList[current.id])
        {
            if (independentSet.contains(neighbor))
            {
                canInclude = false;
                break;
            }
        }

        if (canInclude && (totalError + current.error) < eps)
        {
            independentSet.insert(current.id);
            totalError += current.error;
        }
    }
    for (int i : independentSet)
    {
        auto &tpr = allTPRs[i];
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
    }
    appState.regionsMerged = independentSet.size();
}

void MergePreprocessor::vertexColoring()
{
    int n = allTPRs.size();
    vertexColors.resize(n);
    std::fill(vertexColors.begin(), vertexColors.end(), -1);
    vertexColors[0] = 0;

    for (int u = 1; u < n; ++u)
    {
        std::set<int> usedColors;

        for (int v : adjList[u])
        {
            if (vertexColors[v] != -1)
            {
                usedColors.insert(vertexColors[v]);
            }
        }

        int assignedColor = 0;
        while (usedColors.count(assignedColor))
        {
            ++assignedColor;
        }
        vertexColors[u] = assignedColor;
    }
    std::set<int> uniqueColors(vertexColors.begin(), vertexColors.end());
    appState.conflictGraphStats.numColors = uniqueColors.size();
}

void MergePreprocessor::findIndependentSetWithColoring(float eps)
{
    const int precision = 10000;
    std::unordered_map<int, std::vector<int>> colorClasses;
    for (size_t i = 0; i < vertexColors.size(); ++i)
    {
        colorClasses[vertexColors[i]].push_back(i);
    }

    std::vector<int> bestIS;
    int mostPatches = 0;
    int bestSize = std::numeric_limits<int>::max();
    int scaledThreshold = static_cast<int>(std::round(eps * precision));

    for (const auto &[color, vertices] : colorClasses)
    {
        size_t n = vertices.size();
        std::vector<int> dp(scaledThreshold + 1, 0);
        std::vector<std::vector<int>> selected(n, std::vector<int>(scaledThreshold + 1, 0));

        for (size_t i = 0; i < n; ++i)
        {
            int vertexIndex = vertices[i];
            int numQuads = allTPRs[vertexIndex].getMaxPatches();
            float error = allTPRs[vertexIndex].error;

            int scaledError = static_cast<int>(std::round(error * precision));

            for (int j = scaledThreshold; j >= scaledError; --j)
            {
                if (dp[j - scaledError] + numQuads > dp[j])
                {
                    dp[j] = dp[j - scaledError] + numQuads;
                    selected[i][j] = 1;
                }
            }
        }

        int maxQuads = dp[scaledThreshold];

        std::vector<int> selectedVertices;
        int remainingError = scaledThreshold;
        for (int i = n - 1; i >= 0; --i)
        {
            int currError = static_cast<int>(std::round(allTPRs[vertices[i]].error * precision));
            if (selected[i][remainingError] && remainingError >= currError)
            {
                selectedVertices.push_back(vertices[i]);
                remainingError -= currError;
            }
        }

        if (maxQuads > mostPatches || (maxQuads == mostPatches && selectedVertices.size() < bestSize))
        {
            mostPatches = maxQuads;
            bestSize = selectedVertices.size();
            bestIS = selectedVertices;
        }
    }

    float sum = 0;
    for (int i : bestIS)
    {
        auto &tpr = allTPRs[i];
        sum += tpr.error;
        std::cout << "Error: " << tpr.error << "\n";
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
    }
    std::cout << "sum: " << sum << std::endl;
    appState.regionsMerged = bestIS.size();
}

void MergePreprocessor::mergeEdgeRegion(const Region &region)
{
    auto [rowIdx, colIdx] = region[0];
    auto maxRegion = region[1];

    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    if (maxRegion.first == 0)
    {
        mergeRowWithoutError(colIdx, maxRegion.second);
        return;
    }
    for (int i = 0; i < maxRegion.second; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        if (currRowIdx == -1)
            break;
        rowIdxs.push_back(currRowIdx);
    }

    for (int i = 0; i < rowIdxs.size(); i++)
    {
        mergeRowWithoutError(rowIdxs[i], maxRegion.first);
    }
    mergeRowWithoutError(mesh.edges[rowIdxs[0]].nextIdx, maxRegion.second);
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
            edgeRegions.push_back(EdgeRegion{gridIdxPair, faceIdx});
        }
    }
    std::cout << "size of edge regions" << edgeRegions.size() << std::endl;

    return edgeRegions;
}
