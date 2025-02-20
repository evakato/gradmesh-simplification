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
        printElapsedTime(appState.startTime);
        return;
    }

    if (appState.preprocessSingleMergeProgress == 0)
    {
        std::vector<SingleHalfEdge> boundaryEdges;
        merger.select.findCandidateMerges(&boundaryEdges);
        merger.metrics.setBoundaryEdges(boundaryEdges);
        appState.startTime = std::chrono::high_resolution_clock::now();
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

void MergePreprocessor::mergeMotorcycle()
{
    appState.startTime = std::chrono::high_resolution_clock::now();
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    auto mergeableRegions = merger.metrics.getMergeableRegions();
    for (auto &mr : mergeableRegions)
    {
        merger.metrics.findSumOfErrors(mr);
    }
    std::sort(mergeableRegions.begin(), mergeableRegions.end(),
              [](const MergeableRegion &a, const MergeableRegion &b)
              {
                  // int productA = (a.maxRegion.first + 1) * (a.maxRegion.second + 1);
                  // int productB = (b.maxRegion.first + 1) * (b.maxRegion.second + 1);
                  // return productA > productB;
                  return a.sumOfErrors < b.sumOfErrors;
              });
    for (auto &region : mergeableRegions)
    {
        if (region.maxRegion.first == 0 && region.maxRegion.second == 0)
            continue;

        writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
        mergeEdgeRegionWithError({region.gridPair, region.maxRegion});
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
        }
    }

    appState.regionsMerged = mergeableRegions.size();
    appState.updateMeshRender();
    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
    appState.mergeProcess = MergeProcess::Merging;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - appState.startTime.value();
    std::cout << "time: " << elapsed.count() << std::endl;
    appState.startTime.reset();
}

void MergePreprocessor::preprocessProductRegions()
{
    if (productRegionIdx == 0)
        appState.startTime = std::chrono::high_resolution_clock::now();

    // auto &edgeRegions = appState.edgeRegions;
    auto &currEdgeRegion = edgeRegions[productRegionIdx++];

    auto allRegions = findMaxProductRegion(currEdgeRegion);
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    if (!allRegions.empty())
    {
        for (auto &region : allRegions)
        {
            // region.maxRegionAABB = mesh.getProductRegionAABB(currEdgeRegion.gridPair, region.maxRegion);
        }
        currEdgeRegion.setAndSortAttributes(allRegions);
    }

    appState.preprocessProductRegionsProgress = static_cast<float>(productRegionIdx) / edgeRegions.size();

    if (productRegionIdx >= edgeRegions.size())
    {
        createAdjList();
        computeConflictGraphStats();
        appState.mergeProcess = MergeProcess::Merging;
        appState.preprocessProductRegionsProgress = -2.0f;
        productRegionIdx = 0;
        saveConflictGraphToFile(std::string{TPR_PREPROCESSING_DIR} + "/" + appState.meshname + ".txt", allTPRs, adjList, edgeRegions);

        printElapsedTime(appState.startTime);
    }
}

void MergePreprocessor::loadProductRegionsPreprocessing()
{
    appState.mergeProcess = MergeProcess::Merging;
    loadConflictGraphFromFile(appState.loadPreprocessingFilename, allTPRs, adjList, edgeRegions);
    // createAdjList();
    computeConflictGraphStats();
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
    appState.startTime = std::chrono::high_resolution_clock::now();

    float delta = 0.0001f;
    float minThreshold = 0.0f;
    float maxThreshold = appState.mergeSettings.errorThreshold * 2.0f;
    float bestThreshold;
    int bestFaces = std::numeric_limits<int>::max();
    float bestError = std::numeric_limits<float>::max();

    for (int i = 0; i < 10; i++)
    {
        if (maxThreshold < minThreshold)
            break;
        float currThreshold = minThreshold + (maxThreshold - minThreshold) / 2.0f;
        appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
        greedyQuadErrorHeuristic(currThreshold);
        currIndependentSetIterator = currIndependentSet.begin();
        // writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);
        mergeIndependentSet();
        int numFaces = getValidCompIndices(mesh.faces).size();
        std::cout << "Iteration: " << i
                  << ", minThreshold: " << minThreshold
                  << ", maxThreshold: " << maxThreshold
                  << ", currThreshold: " << currThreshold
                  << ", numFaces: " << numFaces
                  << ", mergeError: " << appState.mergeError
                  << std::endl;
        if (((numFaces < bestFaces) || (numFaces == bestFaces && appState.mergeError < bestError)) && appState.mergeError < appState.mergeSettings.errorThreshold)
        {
            bestThreshold = currThreshold;
            bestFaces = numFaces;
            bestError = appState.mergeError;
            minThreshold = currThreshold + delta;
        }
        else
        {
            maxThreshold = currThreshold + delta;
        }
    }

    std::cout << bestThreshold << std::endl;

    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    greedyQuadErrorHeuristic(bestThreshold);
    currIndependentSetIterator = currIndependentSet.begin();
    mergeIndependentSet();

    appState.mergeProcess = MergeProcess::Merging;
    printElapsedTime(appState.startTime);
}

void MergePreprocessor::mergeGreedyQuadErrorOneStep()
{
    float delta = 0.0001f;
    appState.startTime = std::chrono::high_resolution_clock::now();

    float minThreshold = 0.0f;
    float maxThreshold = appState.mergeSettings.errorThreshold * 2.0f;
    float bestThreshold;
    int bestFaces = std::numeric_limits<int>::max();
    float bestError = std::numeric_limits<float>::max();

    for (int i = 0; i < 10; i++)
    {
        if (maxThreshold < minThreshold)
            break;
        float currThreshold = minThreshold + (maxThreshold - minThreshold) / 2.0f;
        appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
        greedyQuadErrorOneStep(currThreshold);
        currIndependentSetIterator = currIndependentSet.begin();
        // writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);
        mergeIndependentSet();
        int numFaces = getValidCompIndices(mesh.faces).size();
        std::cout << "Iteration: " << i
                  << ", minThreshold: " << minThreshold
                  << ", maxThreshold: " << maxThreshold
                  << ", currThreshold: " << currThreshold
                  << ", numFaces: " << numFaces
                  << ", mergeError: " << appState.mergeError
                  << std::endl;
        if (((numFaces < bestFaces) || (numFaces == bestFaces && appState.mergeError < bestError)) && appState.mergeError < appState.mergeSettings.errorThreshold)
        {
            bestThreshold = currThreshold;
            bestFaces = numFaces;
            bestError = appState.mergeError;
            minThreshold = currThreshold + delta;
        }
        else
        {
            maxThreshold = currThreshold + delta;
        }
    }

    std::cout << bestThreshold << std::endl;

    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    greedyQuadErrorOneStep(bestThreshold);
    currIndependentSetIterator = currIndependentSet.begin();
    mergeIndependentSet();

    appState.mergeProcess = MergeProcess::Merging;
    printElapsedTime(appState.startTime);
}

void MergePreprocessor::mergeIndependentSet()
{
    while (currIndependentSetIterator != currIndependentSet.end())
    {
        auto &tpr = allTPRs[(*currIndependentSetIterator)];
        // std::cout << " it: " << tpr.gridPair.first << ", " << tpr.gridPair.second << "   " << tpr.maxRegion.first << ", " << tpr.maxRegion.second << std::endl;
        writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
        mergeEdgeRegion({tpr.gridPair, tpr.maxRegion});
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
        }
        currIndependentSetIterator = std::next(currIndependentSetIterator);
    }

    appState.updateMeshRender();
    merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
    appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
}

void MergePreprocessor::greedyQuadErrorHeuristic(float eps)
{
    float w = appState.quadErrorWeight;
    float maxError = eps;
    float numFaces = getValidCompIndices(mesh.faces).size();

    auto cmp = [w, numFaces, maxError](const TPRNode &a, const TPRNode &b)
    {
        float scoreA = (w * (a.getMaxPatches() / numFaces) - ((a.error / maxError) * (1.0f - w)));
        float scoreB = (w * (b.getMaxPatches() / numFaces) - ((b.error / maxError) * (1.0f - w)));
        return scoreA < scoreB;
    };

    std::priority_queue<TPRNode, std::vector<TPRNode>, decltype(cmp)> pq(cmp);

    for (const auto &tpr : allTPRs)
    {
        pq.push(tpr);
    }

    currIndependentSet.clear();
    float totalError = 0;

    while (!pq.empty() && totalError < eps)
    {
        auto current = pq.top();
        pq.pop();

        bool canInclude = true;
        for (int neighbor : adjList[current.id])
        {
            if (currIndependentSet.contains(neighbor))
            {
                canInclude = false;
                break;
            }
        }

        if (canInclude && (totalError + current.error) < eps)
        {
            currIndependentSet.insert(current.id);
            totalError += current.error;
        }
    }

    appState.regionsMerged = currIndependentSet.size();
}

bool MergePreprocessor::canInclude(int idx)
{
    for (int neighbor : adjList[idx])
        if (currIndependentSet.contains(neighbor))
            return false;
    return true;
}

void MergePreprocessor::greedyQuadErrorOneStep(float eps)
{
    float w = appState.quadErrorWeight;
    float maxError = eps;
    float numFaces = getValidCompIndices(mesh.faces).size();

    auto cmp = [](const TPRNodePair &a, const TPRNodePair &b)
    {
        return a.score < b.score;
    };

    std::priority_queue<TPRNodePair, std::vector<TPRNodePair>, decltype(cmp)> pq(cmp);
    appState.oneStepQuadErrorProgress = 0.0f;

    for (int i = 0; i < allTPRs.size(); i++)
    {
        const auto &tpr1 = allTPRs[i];
        float scoreA = (w * (tpr1.getMaxPatches() / numFaces) - ((tpr1.error / maxError) * (1.0f - w)));
        float maxScoreB = scoreA;

        for (int j = 0; j < allTPRs.size(); j++)
        {
            if (i == j)
                continue;
            const auto &tpr2 = allTPRs[j];

            bool areAdjacent = false;
            for (int neighbor : adjList[tpr1.id])
            {
                if (neighbor == tpr2.id)
                {
                    areAdjacent = true;
                    break;
                }
            }
            if (areAdjacent)
                continue;

            // float scoreB = (w * (tpr2.getMaxPatches() / numFaces) - ((tpr2.error / maxError) * (1.0f - w)));
            float normalizedPatchesPair = (tpr1.getMaxPatches() + tpr2.getMaxPatches() - 1) / numFaces;
            float normalizedErrorPair = (tpr1.error + tpr2.error) / maxError;
            float score = w * normalizedPatchesPair - (1.0f - w) * normalizedErrorPair;
            maxScoreB = std::max(maxScoreB, score);
        }
        pq.push(TPRNodePair{i, -1, maxScoreB});
    }

    int pqSize = pq.size();

    currIndependentSet.clear();
    float totalError = 0;
    int processedNodes = 0;

    while (!pq.empty() && totalError < eps)
    {
        auto current = pq.top();
        pq.pop();

        if (current.j == -1)
        {
            float nodeError = allTPRs[current.i].error;
            if (canInclude(current.i) && (totalError + nodeError) < eps)
            {
                currIndependentSet.insert(current.i);
                totalError += nodeError;
            }
        }
        else
        {
            float nodeError1 = allTPRs[current.i].error;
            float nodeError2 = allTPRs[current.j].error;
            if (canInclude(current.i) && canInclude(current.j) && (totalError + nodeError1 + nodeError2) < eps)
            {
                currIndependentSet.insert(current.i);
                currIndependentSet.insert(current.j);
                totalError += nodeError1 + nodeError2;
            }
        }
        processedNodes++;
        if (processedNodes % 10000 == 0)
            appState.oneStepQuadErrorProgress = static_cast<int>(processedNodes) / pqSize;
    }

    appState.regionsMerged = currIndependentSet.size();
    appState.oneStepQuadErrorProgress = -1.0f;
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

void MergePreprocessor::mergeEdgeRegionWithError(const Region &region)
{
    auto [adj1, adj2] = region[0];
    auto maxRegion = region[1];

    std::vector<int> rowIdxs = {adj1};
    int currRowIdx = adj1;

    if (maxRegion.first == 0)
    {
        int currEdgeIdx = adj2;
        for (int i = 0; i < maxRegion.second; i++)
        {
            auto &currEdge = mesh.edges[currEdgeIdx];
            if (!mesh.validMergeEdge(currEdge))
                break;

            writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
            AABB aabb;
            float mergeError = merger.attemptMerge(currEdgeIdx, aabb);
            if (mergeError > appState.mergeSettings.errorThreshold)
            {
                mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
                int twinIdx = mesh.edges[currEdgeIdx].twinIdx;
                if (twinIdx != -1)
                    currEdgeIdx = mesh.getFaceEdgeIdxs(twinIdx)[2];
            }
        }
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
        }
        return;
    }
    for (int i = 0; i < maxRegion.second; i++)
    {
        currRowIdx = mesh.getNextRowIdx(currRowIdx);
        if (currRowIdx == -1)
            break;
        rowIdxs.push_back(currRowIdx);
    }
    std::set<int> verticalEdgeIdxs;
    for (int firstRowIdx : rowIdxs)
    {
        int currEdgeIdx = firstRowIdx;
        for (int i = 0; i < maxRegion.first; i++)
        {
            auto &currEdge = mesh.edges[currEdgeIdx];
            if (!mesh.validMergeEdge(currEdge))
                break;

            writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
            AABB aabb;
            float mergeError = merger.attemptMerge(currEdgeIdx, aabb);
            verticalEdgeIdxs.insert(mesh.edges[currEdgeIdx].prevIdx);
            if (mergeError > appState.mergeSettings.errorThreshold)
            {
                mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
                int twinIdx = mesh.edges[currEdgeIdx].twinIdx;
                if (twinIdx != -1)
                    currEdgeIdx = mesh.getFaceEdgeIdxs(twinIdx)[2];
            }
        }
    }
    if (maxRegion.second == 0)
        return;

    for (int verticalEdgeIdx : verticalEdgeIdxs)
    {
        for (int i = 0; i < maxRegion.second; i++)
        {
            auto &currEdge = mesh.edges[verticalEdgeIdx];
            if (!mesh.validMergeEdge(currEdge))
                break;

            writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
            AABB aabb;
            float mergeError = merger.attemptMerge(verticalEdgeIdx, aabb);
            if (mergeError > appState.mergeSettings.errorThreshold)
            {
                mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
                break;
            }
        }
    }
}

void MergePreprocessor::mergeEdgeRegion(const Region &region)
{
    writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
    auto [rowIdx, colIdx] = region[0];
    auto maxRegion = region[1];

    std::vector<int> rowIdxs = {rowIdx};
    int currRowIdx = rowIdx;
    if (maxRegion.first == 0)
    {
        mergeRowWithoutError(colIdx, maxRegion.second);
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
        }
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
        // if (!mesh.edges[rowIdxs[i]].isValid())
        // std::cout << "invalid" << std::endl;
        mergeRowWithoutError(rowIdxs[i], maxRegion.first);
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
            return;
        }
    }
    mergeRowWithoutError(mesh.edges[rowIdxs[0]].nextIdx, maxRegion.second);
    // if (!mesh.edges[mesh.edges[rowIdxs[0]].nextIdx].isValid())
    // std::cout << "invalid" << std::endl;
}

void MergePreprocessor::setEdgeRegions()
{
    meshCornerFaces = mesh.findCornerFaces();
    edgeRegions.clear();
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
