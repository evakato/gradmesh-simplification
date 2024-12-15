#include "preprocessing.hpp"

// #define ENABLE_DEBUG_PRINTS

#ifdef ENABLE_DEBUG_PRINTS
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

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

void MergePreprocessor::preprocessProductRegions()
{
    // auto &edgeRegions = appState.edgeRegions;
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
        computeConflictGraphStats();
        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessProductRegionsProgress = -2.0f;
        productRegionIdx = 0;
        saveConflictGraphToFile("conflictgraph.txt", allTPRs, adjList, edgeRegions);
    }
}

void MergePreprocessor::loadProductRegionsPreprocessing()
{
    loadConflictGraphFromFile("conflictgraph.txt", allTPRs, adjList, edgeRegions);
    createAdjList();
    vertexColoring();
    computeConflictGraphStats();
    appState.mergeProcess = MergeProcess::Merging;
    appState.preprocessProductRegionsProgress = -2.0f;
    productRegionIdx = 0;
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
            break;
        std::cout << currEdgeIdx << std::endl;
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
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");

    auto mergedCols = mergeRow(colIdx, errorAABB, false);
    std::ranges::copy(mergedCols, std::back_inserter(regionAttributes));
    int colLength = mergedCols.empty() ? 0 : mergedCols.back().maxRegion.second;
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");

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
            mesh = readHemeshFile("mesh_saves/save_0.hemesh");
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

void MergePreprocessor::createAdjList()
{
    for (auto &edgeRegion : edgeRegions)
    {
        std::ranges::copy(edgeRegion.generateAllTPRs(), std::back_inserter(allTPRs));
    }

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
    for (int i = 0; i < allTPRs.size(); i++)
    {
        allTPRs[i].id = i;
        allTPRs[i].degree = adjList[i].size();
    }
}

void MergePreprocessor::computeConflictGraphStats()
{
    int degreeSum = 0;
    for (const auto &tpr : allTPRs)
    {
        degreeSum += tpr.degree;
    }

    appState.conflictGraphStats.numOfNodes = allTPRs.size();
    appState.conflictGraphStats.avgDegree = static_cast<float>(degreeSum) / allTPRs.size();
    float totalError = 0.0f;
    int totalQuads = 0;
    for (const auto &node : allTPRs)
    {
        totalError += node.error;
        totalQuads += node.getMaxPatches();
    }

    appState.conflictGraphStats.avgError = allTPRs.empty() ? 0.0f : totalError / allTPRs.size();
    appState.conflictGraphStats.avgQuads = allTPRs.empty() ? 0 : static_cast<float>(totalQuads) / allTPRs.size();
}

void MergePreprocessor::mergeGreedyQuadError()
{
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    createConflictGraph(appState.mergeSettings.errorThreshold);
    appState.updateMeshRender();
    // writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);
    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
    appState.mergeProcess = MergeProcess::Merging;
}

void MergePreprocessor::mergeTPRs()
{
    //  createConflictGraph(allTPRs, appState.mergeSettings.errorThreshold);
    // findIndependentSetWithColoring(appState.mergeSettings.errorThreshold);
    // heuristicBinarySearch(appState.mergeSettings.errorThreshold);
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    greedyQuadErrorTwoStep(appState.mergeSettings.errorThreshold);
    appState.updateMeshRender();
    // writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);

    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);

    appState.mergeProcess = MergeProcess::Merging;
}

void MergePreprocessor::heuristicBinarySearch(float userError)
{
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    findIndependentSetWithColoring(userError);
    appState.updateMeshRender();
    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    float totalError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
    if (totalError < userError)
        return;

    float low = 0.0f;
    float high = userError;
    float bestEps = 0.0f;

    for (int i = 0; i < 10; i++)
    {
        float eps = (low + high) / 2;
        appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
        findIndependentSetWithColoring(eps);
        appState.updateMeshRender();
        merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
        float totalError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
        std::cout << eps << " " << totalError << std::endl;
        if (totalError < userError)
        {
            low = eps;
            bestEps = eps;
        }
        else
        {
            high = eps;
        }
    }
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    findIndependentSetWithColoring(bestEps);
}

void MergePreprocessor::createConflictGraph(float eps)
{
    float w = appState.quadErrorWeight;
    float numFaces = static_cast<float>(getValidCompIndices(mesh.getFaces()).size());
    float maxError = 0;

    DEBUG_PRINT("Debug: quadErrorWeight = " << w << ", numFaces = " << numFaces);

    for (const auto &tpr : allTPRs)
    {
        if (tpr.error > maxError)
            maxError = tpr.error;
    }
    DEBUG_PRINT("Debug: maxError = " << maxError);

    auto cmp = [w, numFaces, maxError](const TPRNode &a, const TPRNode &b)
    {
        float scoreA = w * (a.getMaxPatches() / numFaces - (a.error / maxError) * (1.0f - w));
        float scoreB = w * (b.getMaxPatches() / numFaces - (b.error / maxError) * (1.0f - w));
        return scoreA < scoreB;
    };

    std::priority_queue<TPRNode, std::vector<TPRNode>, decltype(cmp)> pq(cmp);

    for (const auto &tpr : allTPRs)
    {
        pq.push(tpr);
        DEBUG_PRINT("Debug: Pushed TPRNode [id=" << tpr.id
                                                 << ", error=" << tpr.error
                                                 << ", maxRegion=(" << tpr.maxRegion.first << ", " << tpr.maxRegion.second << ")"
                                                 << ", getMaxPatches()=" << tpr.getMaxPatches() << "]");
    }

    std::set<int> independentSet;
    float totalError = 0;

    while (!pq.empty() && totalError < eps)
    {
        auto current = pq.top();
        pq.pop();

        DEBUG_PRINT("Debug: Processing TPRNode [id=" << current.id
                                                     << ", error=" << current.error
                                                     << ", maxRegion=" << current.maxRegion.first << " " << current.maxRegion.second << "]");

        bool canInclude = true;
        for (int neighbor : adjList[current.id])
        {
            if (independentSet.contains(neighbor))
            {
                canInclude = false;
                DEBUG_PRINT("Debug: Conflict with neighbor " << neighbor);
                break;
            }
        }

        if (canInclude && (totalError + current.error) < eps)
        {
            independentSet.insert(current.id);
            totalError += current.error;
            DEBUG_PRINT("Debug: Added TPRNode [id=" << current.id << "] to independentSet. TotalError = " << totalError);
        }
    }

    for (int i : independentSet)
    {
        auto &tpr = allTPRs[i];
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});

        DEBUG_PRINT("Debug: Merged TPRNode [id=" << i
                                                 << "] with gridPair=" << tpr.gridPair.first << ", " << tpr.gridPair.second
                                                 << " and maxRegion=(" << tpr.maxRegion.first << ", " << tpr.maxRegion.second << ")");
    }

    appState.regionsMerged = independentSet.size();

    DEBUG_PRINT("Debug: Regions Merged = " << appState.regionsMerged
                                           << ", TotalError = " << totalError
                                           << ", Independent Set Size = " << independentSet.size());
}

bool cannotAdd(const std::vector<int> &overlapList, const std::set<int> &independentSet)
{
    for (int neighbor : overlapList)
    {
        if (independentSet.contains(neighbor))
        {
            return true;
        }
    }
    return false;
}

void MergePreprocessor::greedyQuadErrorTwoStep(float userError)
{
    float w = appState.quadErrorWeight;

    float numFaces = static_cast<float>(getValidCompIndices(mesh.getFaces()).size());
    auto cmp = [w, numFaces](const TPRNode &a, const TPRNode &b)
    { return w * (a.getMaxPatches() / numFaces - a.error * (1.0f - w)) < w * (b.getMaxPatches() / numFaces - b.error * (1.0f - w)); };

    std::set<int> independentSet;
    float eps = userError;

    while (true)
    {
        int bestQuads = 0;
        std::vector<int> bestSet;
        for (int i = 0; i < allTPRs.size(); i++)
        {
            if (independentSet.contains(i))
                continue;
            const auto &current = allTPRs[i];
            if (cannotAdd(adjList[current.id], independentSet))
                continue;

            const auto &currentAdjList = adjList[current.id];

            for (int j = i + 1; j < allTPRs.size(); j++)
            {
                if (independentSet.contains(j) || std::find(currentAdjList.begin(), currentAdjList.end(), j) != currentAdjList.end())
                    continue;
                const auto &secondCurrent = allTPRs[j];
                if (cannotAdd(adjList[secondCurrent.id], independentSet))
                    continue;
                const auto &secondCurrentAdjList = adjList[secondCurrent.id];

                /*
                                for (int k = j + 1; k < allTPRs.size(); k++)
                                {
                                    if (independentSet.contains(k) || std::find(currentAdjList.begin(), currentAdjList.end(), k) != currentAdjList.end() || std::find(secondCurrentAdjList.begin(), secondCurrentAdjList.end(), k) != secondCurrentAdjList.end())
                                        continue;
                                    const auto &thirdCurrent = allTPRs[k];
                                    if (cannotAdd(adjList[thirdCurrent.id], independentSet))
                                        continue;

                                    int totalQuads = current.getMaxPatches() + secondCurrent.getMaxPatches() + thirdCurrent.getMaxPatches() - 3;
                                    float totalError = current.error + secondCurrent.error + thirdCurrent.error;
                                    if (totalError <= eps && totalQuads > bestQuads)
                                    {
                                        bestQuads = totalQuads;
                                        bestSet = {i, j, k};
                                    }
                                }
                                */
                int totalQuads = current.getMaxPatches() + secondCurrent.getMaxPatches() - 2;
                float totalError = current.error + secondCurrent.error;
                if (totalError <= eps && totalQuads > bestQuads)
                {
                    bestQuads = totalQuads;
                    bestSet = {i, j};
                }
            }
            int totalQuads = current.getMaxPatches() - 1;
            if (current.error <= eps && totalQuads > bestQuads)
            {
                bestQuads = totalQuads;
                bestSet = {i};
            }
        }
        if (bestSet.empty())
            break;
        for (int q : bestSet)
        {
            independentSet.insert(q);
            eps -= allTPRs[q].error;
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

int MergePreprocessor::binarySearch(const std::vector<int> &arr, int left, float eps)
{
    int right = arr.size() - 1;
    int bestMerge = -1;
    writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);

    while (left <= right)
    {
        mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
        int mid = left + (right - left) / 2;
        const auto &tpr = allTPRs[arr[mid]];
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
        appState.updateMeshRender();
        merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
        float mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);

        if (mergeError < eps)
        {
            std::cout << "valid: " << mergeError << " " << tpr.getMaxPatches() << " ";
            right = mid - 1; // Search in the left half
            bestMerge = mid;
        }
        else
        {
            std::cout << "not valid: " << mergeError << " " << tpr.getMaxPatches() << " ";
            left = mid + 1; // Search in the right half
        }
    }
    mesh = readHemeshFile("mesh_saves/lastsave.hemesh");

    return bestMerge;
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
    float bestSimplification = 0.0f;
    // int bestSize = std::numeric_limits<int>::max();
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

        if (static_cast<float>(maxQuads) / selectedVertices.size() > bestSimplification)
        {
            bestSimplification = static_cast<float>(maxQuads) / selectedVertices.size();
            bestIS = selectedVertices;
        }
    }

    float sum = 0;
    for (int i : bestIS)
    {
        auto &tpr = allTPRs[i];
        sum += tpr.error;
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
    }
    appState.regionsMerged = bestIS.size();
}

/*
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

    for (auto &[color, colorVerts] : colorClasses)
    {
        size_t n = colorVerts.size();
        auto compareByPatches = [&](int v1, int v2)
        {
            return allTPRs[v1].getMaxPatches() > allTPRs[v2].getMaxPatches();
        };
        std::sort(colorVerts.begin(), colorVerts.end(), compareByPatches);
        std::cout << "Sorted values (descending):\n";
        for (int v : colorVerts)
        {
            std::cout << "Vertex: " << v << ", MaxPatches: " << allTPRs[v].getMaxPatches() << '\n';
        }

        int left = -1;
        std::vector<int> selected;
        for (int i = 0; i < colorVerts.size(); i++)
        {
            const auto &tpr = allTPRs[colorVerts[i]];
            if (tpr.error <= eps)
            {
                selected.push_back(colorVerts[i]);
                mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
                left = i + 1;
                std::cout << tpr.error << " " << tpr.getMaxPatches() << " ";
                break;
            }
        }
        if (left == -1)
            continue;

        std::cout << "Did first merge" << std::endl;

        int count = 1;
        while (left <= colorVerts.size())
        {
            int sec = binarySearch(colorVerts, left, eps);
            if (sec == -1)
                break;
            std::cout << ++count << std::endl;
            selected.push_back(colorVerts[sec]);
            const auto &tpr = allTPRs[colorVerts[sec]];
            mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
            left = sec + 1;
        }
        int maxQuads = 0;
        for (int selectedIdx : selected)
        {
            const auto &tpr = allTPRs[selectedIdx];
            maxQuads += tpr.getMaxPatches();
            // std::cout << tpr.getMaxPatches() << " ";
        }
        std::cout << "\n";
        mesh = readHemeshFile("mesh_saves/save_0.hemesh");

        if (maxQuads > mostPatches || (maxQuads == mostPatches && selected.size() < bestSize))
        {
            mostPatches = maxQuads;
            bestSize = selected.size();
            bestIS = selected;
        }
    }

    for (int i : bestIS)
    {
        auto &tpr = allTPRs[i];
        std::cout << tpr.getMaxPatches() << " " << tpr.error << " ";
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
    }
    appState.updateMeshRender();
    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    float mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
    std::cout << "HUGE TEST " << mergeError << std::endl;

    appState.regionsMerged = bestIS.size();
}
*/

void MergePreprocessor::mergeEdgeRegion(const Region &region)
{
    auto [rowIdx, colIdx] = region[0];
    auto maxRegion = region[1];
    std::cout << rowIdx << " " << colIdx << std::endl;
    std::cout << maxRegion.first << " " << maxRegion.second << std::endl;

    if (maxRegion.first == 0)
    {
        mergeRowWithoutError(colIdx, maxRegion.second);
        return;
    }
    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    int mergeColIdx = mesh.edges[rowIdx].nextIdx;
    std::cout << mergeColIdx << std::endl;

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
    mergeRowWithoutError(mergeColIdx, maxRegion.second);
}

void MergePreprocessor::setEdgeRegions()
{
    meshCornerFaces = mesh.findCornerFaces();
    getEdgeRegions(meshCornerFaces);
}

std::vector<EdgeRegion> MergePreprocessor::getEdgeRegions(const std::vector<std::pair<int, int>> &startPairs)
{
    // std::vector<EdgeRegion> edgeRegions;
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
    std::cout << "size of edge regions: " << edgeRegions.size() << std::endl;

    return edgeRegions;
}
