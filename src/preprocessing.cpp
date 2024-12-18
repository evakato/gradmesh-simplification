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
        std::vector<SingleHalfEdge> boundaryEdges;
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

void MergePreprocessor::mergeMotorcycle()
{
    mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    auto mergeableRegions = merger.metrics.getMergeableRegions();
    std::sort(mergeableRegions.begin(), mergeableRegions.end(),
              [](const MergeableRegion &a, const MergeableRegion &b)
              {
                  int productA = (a.maxRegion.first + 1) * (a.maxRegion.second + 1);
                  int productB = (b.maxRegion.first + 1) * (b.maxRegion.second + 1);
                  return productA > productB;
              });
    for (auto &region : mergeableRegions)
    {
        if (region.maxRegion.first == 0 && region.maxRegion.second == 0)
            continue;

        writeHemeshFile("mesh_saves/lastsave.hemesh", mesh);
        mergeEdgeRegion({region.gridPair, region.maxRegion});
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
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    greedyQuadErrorHeuristic(appState.mergeSettings.errorThreshold);
    currIndependentSetIterator = currIndependentSet.begin();
    // writeHemeshFile("mesh_saves/save_" + std::to_string(++productRegionIteration) + ".hemesh", mesh);
    mergeIndependentSet();
    appState.mergeProcess = MergeProcess::Merging;
}

void MergePreprocessor::mergeTPRs()
{
    //  greedyQuadErrorHeuristic(allTPRs, appState.mergeSettings.errorThreshold);
    appState.mesh = readHemeshFile("mesh_saves/save_0.hemesh");
    greedyQuadErrorTwoStep(appState.mergeSettings.errorThreshold);
    currIndependentSetIterator = currIndependentSet.begin();
    mergeIndependentSet();
    appState.mergeProcess = MergeProcess::Merging;
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
    appState.mergeProcess = MergeProcess::Merging;
}

void MergePreprocessor::greedyQuadErrorHeuristic(float eps)
{
    float w = appState.quadErrorWeight;
    float numFaces = 0.0f;
    float maxError = eps;

    for (const auto &tpr : allTPRs)
    {
        if (tpr.getMaxPatches() > numFaces)
            numFaces = static_cast<float>(tpr.getMaxPatches());
    }

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

void MergePreprocessor::greedyQuadErrorTwoStep(float eps)
{
    float w = appState.quadErrorWeight;
    float numFaces = 0.0f;
    float maxError = eps;

    for (const auto &tpr : allTPRs)
    {
        if (tpr.getMaxPatches() > numFaces)
            numFaces = static_cast<float>(tpr.getMaxPatches());
    }

    auto cmp = [](const TPRNodePair &a, const TPRNodePair &b)
    {
        return a.score < b.score;
    };

    std::priority_queue<TPRNodePair, std::vector<TPRNodePair>, decltype(cmp)> pq(cmp);
    appState.oneStepQuadErrorProgress = 0.0f;

    for (const auto &tpr : allTPRs)
    {
        float score = w * ((tpr.getMaxPatches() - 1) / numFaces - (tpr.error / maxError) * (1.0f - w));
        pq.push(TPRNodePair{tpr.id, -1, score});
    }
    for (int i = 0; i < allTPRs.size(); i++)
    {
        for (int j = i + 1; j < allTPRs.size(); j++)
        {
            const auto &tpr1 = allTPRs[i];
            const auto &tpr2 = allTPRs[j];
            float score = w * ((tpr1.getMaxPatches() + tpr2.getMaxPatches() - 2) / (numFaces * 2) - ((tpr1.error + tpr2.error) / maxError) * (1.0f - w));
            pq.push(TPRNodePair{i, j, score});
        }
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
/*
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
/*
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
*/

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
        if (!mesh.edges[rowIdxs[i]].isValid())
            std::cout << "invalid" << std::endl;
        mergeRowWithoutError(rowIdxs[i], maxRegion.first);
        if (!mesh.generatePatches())
        {
            mesh = readHemeshFile("mesh_saves/lastsave.hemesh");
            return;
        }
    }
    mergeRowWithoutError(mesh.edges[rowIdxs[0]].nextIdx, maxRegion.second);
    if (!mesh.edges[mesh.edges[rowIdxs[0]].nextIdx].isValid())
        std::cout << "invalid" << std::endl;
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
