#include "merge_metrics.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

MergeMetrics::MergeMetrics(Params params)
    : mesh(params.mesh),
      mergeSettings(params.mergeSettings),
      patchRenderResources(params.patchRenderResources)
{
    glGenFramebuffers(1, &unmergedFbo);
    glGenFramebuffers(1, &mergedFbo);
}

void MergeMetrics::markTwoHalfEdges(int idx1, int idx2)
{
    auto &lookup = lookupValenceVertex[idx1];
    valenceVertices[lookup.first].setMarked(lookup.second);
    auto &lookup2 = lookupValenceVertex[idx2];
    valenceVertices[lookup2.first].setMarked(lookup2.second);
}

bool MergeMetrics::isMarked(int halfEdgeIdx)
{
    auto &lookup = lookupValenceVertex[halfEdgeIdx];
    return valenceVertices[lookup.first].markedEdges[lookup.second] == 1;
}

void MergeMetrics::findSumOfErrors(MergeableRegion &mr)
{
    auto [rowIdx, colIdx] = mr.gridPair;
    auto [width, length] = mr.maxRegion;
    if (width == 0 && length == 0)
    {
        mr.sumOfErrors = 0;
        return;
    }
    float sum = 0.0f;
    if (width == 0)
    {
        int currIdx = colIdx;
        for (int i = 0; i < length; i++)
        {
            assert(halfEdgeErrors[currIdx] != -2.0f);
            sum += halfEdgeErrors[currIdx];
            currIdx = mesh.getFaceEdgeIdxs(mesh.edges[currIdx].twinIdx)[2];
        }
        mr.sumOfErrors = sum;
        return;
    }
    if (length == 0)
    {
        int currIdx = rowIdx;
        for (int i = 0; i < width; i++)
        {
            assert(halfEdgeErrors[currIdx] != -2.0f);
            sum += halfEdgeErrors[currIdx];
            currIdx = mesh.getFaceEdgeIdxs(mesh.edges[currIdx].twinIdx)[2];
        }
        mr.sumOfErrors = sum;
        return;
    }
    int currIdx = rowIdx;
    for (int j = 0; j < length + 1; j++)
    {
        int nextIdx = mesh.getNextRowIdx(currIdx);
        for (int i = 0; i < width; i++)
        {
            assert(halfEdgeErrors[currIdx] != -2.0f);
            sum += halfEdgeErrors[currIdx];
            currIdx = mesh.getFaceEdgeIdxs(mesh.edges[currIdx].twinIdx)[2];
        }
        currIdx = nextIdx;
    }
    currIdx = colIdx;
    for (int j = 0; j < length; j++)
    {
        int nextIdx = mesh.getFaceEdgeIdxs(mesh.edges[currIdx].twinIdx)[2];
        for (int i = 0; i < width + 1; i++)
        {
            assert(halfEdgeErrors[currIdx] != -2.0f);
            sum += halfEdgeErrors[currIdx];
            currIdx = mesh.edges[mesh.edges[mesh.edges[currIdx].prevIdx].twinIdx].prevIdx;
        }
        currIdx = nextIdx;
    }
    mr.sumOfErrors = sum;
}

void MergeMetrics::getMergeableRegion(std::vector<int> &alreadyVisited, std::vector<MergeableRegion> &mergeableRegions, int halfEdgeIdx)
{
    if (std::find(alreadyVisited.begin(), alreadyVisited.end(), halfEdgeIdx) != alreadyVisited.end())
        return;
    alreadyVisited.push_back(halfEdgeIdx);
    int rowIdx = mesh.edges[halfEdgeIdx].nextIdx;
    int colIdx = mesh.edges[rowIdx].nextIdx;
    int currIdx = rowIdx;
    std::pair<int, int> maxRegion = {0, 0};
    while (!isMarked(currIdx))
    {
        maxRegion.first++;
        int twin = mesh.edges[currIdx].twinIdx;
        if (twin == -1)
            break;
        currIdx = mesh.edges[mesh.edges[twin].nextIdx].nextIdx;
    }
    alreadyVisited.push_back(currIdx);
    currIdx = mesh.edges[currIdx].nextIdx;
    while (!isMarked(currIdx))
    {
        maxRegion.second++;
        int twin = mesh.edges[currIdx].twinIdx;
        if (twin == -1)
            break;
        currIdx = mesh.edges[mesh.edges[twin].nextIdx].nextIdx;
    }
    alreadyVisited.push_back(currIdx);
    currIdx = mesh.edges[currIdx].nextIdx;
    while (!isMarked(currIdx))
    {
        int twin = mesh.edges[currIdx].twinIdx;
        if (twin == -1)
            break;
        currIdx = mesh.edges[mesh.edges[twin].nextIdx].nextIdx;
    }
    alreadyVisited.push_back(currIdx);
    mergeableRegions.push_back({{rowIdx, colIdx}, maxRegion});
}

std::vector<MergeableRegion> MergeMetrics::getMergeableRegions()
{
    std::vector<MergeableRegion> mergeableRegions;
    std::vector<int> alreadyVisited;
    for (const auto &v : valenceVertices)
    {
        if (v.sumMarked() == 4)
        {
            for (int i = 0; i < 4; i++)
            {
                int incidentEdge = v.halfEdgeIdxs[i];
                getMergeableRegion(alreadyVisited, mergeableRegions, incidentEdge);
            }
            continue;
        }
        else if (v.isBoundary() && v.sumMarked() == 2)
        {
            for (int i = 0; i < 4; i++)
            {
                if (v.halfEdgeIdxs[i] != -1)
                {
                    getMergeableRegion(alreadyVisited, mergeableRegions, v.halfEdgeIdxs[i]);
                }
            }
            continue;
        }
        int cornerHalfEdgeIdx = v.isCorner();
        if (cornerHalfEdgeIdx)
        {
            getMergeableRegion(alreadyVisited, mergeableRegions, cornerHalfEdgeIdx);
        }
    }
    return mergeableRegions;
}

void MergeMetrics::generateMotorcycleGraph()
{
    for (auto &v : valenceVertices)
    {
        for (int i = 0; i < 4; i++)
        {
            int edgeIdx = v.halfEdgeIdxs[i];
            if (halfEdgeErrors[edgeIdx] > mergeSettings.singleMergeErrorThreshold || halfEdgeErrors[edgeIdx] == -2.0f)
                v.markedEdges[i] = 1;
            else
                v.markedEdges[i] = 0;
        }
    }
    motorcycleEdges.clear();

    while (true)
    {
        bool didntGrow = true;
        for (auto &v : valenceVertices)
        {
            if (v.isBoundary() || v.sumMarked() == 0 || v.sumMarked() == 3 || v.sumMarked() == 4)
                continue;

            int valenceOneIdx = v.valenceOne();
            if (valenceOneIdx != -1)
            {
                int valenceOne = v.halfEdgeIdxs[valenceOneIdx];
                int newDheIdx = findDoubleHalfEdgeIndex(edgeErrors, valenceOne);
                assert(newDheIdx != -1);
                auto newDhe = edgeErrors[newDheIdx];
                motorcycleEdges.push_back(newDhe);
                markTwoHalfEdges(newDhe.halfEdgeIdx1, newDhe.halfEdgeIdx2);
                didntGrow = false;
                continue;
            }
            int valenceTwoLIdx = v.valenceTwoL();
            if (valenceTwoLIdx != -1)
            {
                int valenceTwoL = v.halfEdgeIdxs[valenceTwoLIdx];
                int newDheIdx = findDoubleHalfEdgeIndex(edgeErrors, valenceTwoL);
                assert(newDheIdx != -1);
                auto newDhe = edgeErrors[newDheIdx];
                motorcycleEdges.push_back(newDhe);
                markTwoHalfEdges(newDhe.halfEdgeIdx1, newDhe.halfEdgeIdx2);
                didntGrow = false;
                continue;
            }
        }
        if (didntGrow)
            break;
    }
}

void MergeMetrics::setEdgeErrorMap(const std::vector<DoubleHalfEdge> &dhes)
{
    edgeErrorPatches = mesh.generatePatches().value();
    edgeErrors = dhes;
    minMaxError.x = std::numeric_limits<float>::max();
    minMaxError.y = std::numeric_limits<float>::min();

    halfEdgeErrors.resize(mesh.edges.size());
    for (const auto &dhe : dhes)
    {
        minMaxError.x = std::min(minMaxError.x, dhe.error);
        minMaxError.y = std::max(minMaxError.y, dhe.error);
        halfEdgeErrors[dhe.halfEdgeIdx1] = dhe.error;
        halfEdgeErrors[dhe.halfEdgeIdx2] = dhe.error;
    }
    for (const auto &be : boundaryEdges)
    {
        halfEdgeErrors[be.halfEdgeIdx] = -2.0f;
    }

    // mergeSettings.singleMergeErrorThreshold = mergeSettings.errorThreshold / edgeErrors.size();
    generateMotorcycleGraph();
}

void MergeMetrics::setValenceVertices()
{
    for (const auto &face : mesh.faces)
    {
        if (!face.isValid())
            continue;
        const auto &edgeIdxs = mesh.getFaceEdgeIdxs(face.halfEdgeIdx);
        for (int idx : edgeIdxs)
        {
            const auto &e = mesh.edges[idx];
            mesh.points[e.originIdx].halfEdgeIdx = idx;
        }
    }
    valenceVertices.resize(mesh.points.size());
    int i = 0;
    for (const auto &point : mesh.points)
    {
        valenceVertices[i].id = i;
        auto &currEdges = valenceVertices[i].halfEdgeIdxs;
        currEdges[0] = point.halfEdgeIdx;
        int twinCcw = mesh.edges[currEdges[0]].twinIdx;
        currEdges[1] = twinCcw == -1 ? -1 : mesh.edges[twinCcw].nextIdx;

        int cw = mesh.edges[mesh.edges[currEdges[0]].prevIdx].twinIdx;
        currEdges[3] = cw;

        if (currEdges[1] == -1 && currEdges[3] == -1)
            currEdges[2] = -1;
        else if (currEdges[1] == -1)
        {
            int cwAgain = mesh.edges[mesh.edges[currEdges[3]].prevIdx].twinIdx;
            currEdges[2] = cwAgain;
        }
        else
        {
            int twinCcwAgain = mesh.edges[currEdges[1]].twinIdx;
            currEdges[2] = twinCcwAgain == -1 ? -1 : mesh.edges[twinCcwAgain].nextIdx;
        }

        i++;
    }

    for (auto &v : valenceVertices)
    {
        for (int i = 0; i < 4; i++)
        {
            int edgeIdx = v.halfEdgeIdxs[i];
            lookupValenceVertex[edgeIdx] = {v.id, i};
        }
    }
}

void MergeMetrics::generateEdgeErrorMap(EdgeErrorDisplay edgeErrorDisplay)
{
    // auto patches = mesh.generatePatches().value();
    static float prevThreshold = 0.0f;
    if (prevThreshold != mergeSettings.singleMergeErrorThreshold)
    {
        generateMotorcycleGraph();
        prevThreshold = mergeSettings.singleMergeErrorThreshold;
    }
    for (const auto &dhe : edgeErrors)
    {
        glm::vec3 col;
        switch (edgeErrorDisplay)
        {
        case EdgeErrorDisplay::Binary:
            col = dhe.error >= (mergeSettings.singleMergeErrorThreshold) ? glm::vec3{1.0f, 0.0f, 0.0f} : black;
            break;
        case EdgeErrorDisplay::Normalized:
            col = glm::vec3{dhe.error / (minMaxError.y - minMaxError.x), 0.0f, 0.0f};
            break;
        case EdgeErrorDisplay::Scaled:
            col = glm::vec3{dhe.error * 100.0f, 0.0f, 0.0f};
            break;
        }
        edgeErrorPatches[dhe.curveId1.patchId].setCurveSelected(dhe.curveId1.curveId, col);
        edgeErrorPatches[dhe.curveId2.patchId].setCurveSelected(dhe.curveId2.curveId, col);
    }
    if (mergeSettings.showMotorcycleEdges)
    {
        for (const auto &dhe : motorcycleEdges)
        {
            edgeErrorPatches[dhe.curveId1.patchId].setCurveSelected(dhe.curveId1.curveId, green);
            edgeErrorPatches[dhe.curveId2.patchId].setCurveSelected(dhe.curveId2.curveId, green);
        }
    }
    for (const auto &be : boundaryEdges)
    {
        const auto &id = be.curveId;
        edgeErrorPatches[id.patchId].setCurveSelected(id.curveId, blue);
    }
    auto glCurveData = getAllPatchGLData(edgeErrorPatches, &Patch::getCurveData);
    int poolRes = 1000;
    setupFBO(patchRenderResources.unmergedTexture, unmergedFbo, poolRes, poolRes);
    glLineWidth(3.5f);
    drawPrimitive(glCurveData, patchRenderResources.curveShaderId, mergeSettings.globalPaddedAABB, VERTS_PER_CURVE);
    writeToImage(poolRes, EDGE_MAP_IMG);
    closeFBO();
}

void MergeMetrics::setAABB()
{
    AABB newAABB = mesh.getMeshAABB();
    // newAABB.resizeToSquare();
    std::cout << newAABB << std::endl;
    mergeSettings.globalAABB = newAABB;

    newAABB.addPadding(mergeSettings.aabbPadding);
    newAABB.ensureSize(MIN_AABB_SIZE);
    mergeSettings.aabb = newAABB;
    mergeSettings.globalPaddedAABB = newAABB;
    mergeSettings.globalAABBRes = newAABB.getRes(mergeSettings.poolRes);
}

void MergeMetrics::captureGlobalImage(const std::vector<GLfloat> &glPatches, const char *imgPath)
{
    FBtoImgParams params = {
        .texture = patchRenderResources.unmergedTexture,
        .fbo = unmergedFbo,
        .width = mergeSettings.globalAABBRes.first,
        .height = mergeSettings.globalAABBRes.second,
        .imgPath = imgPath,
        .glPatches = glPatches,
        .shaderId = patchRenderResources.patchShaderId,
        .aabb = mergeSettings.globalPaddedAABB};

    FBtoImg(params);
}

void MergeMetrics::captureBeforeMerge(const std::vector<GLfloat> &glPatches, AABB &aabb)
{
    if (mergeSettings.pixelRegion == PixelRegion::Global)
        return;

    aabb.addPadding(mergeSettings.aabbPadding);
    aabb.ensureSize(MIN_AABB_SIZE);
    mergeSettings.aabb = aabb;
    // aabb.resizeToSquare();

    // float wRatio = aabb.width() / mergeSettings.globalPaddedAABB.width();
    // float lRatio = aabb.length() / mergeSettings.globalPaddedAABB.length();
    // mergeSettings.aabbRes = {mergeSettings.globalAABBRes.first * wRatio, mergeSettings.globalAABBRes.second * lRatio};
    mergeSettings.aabbRes = mergeSettings.globalAABBRes;
    FBtoImgParams params = {
        .texture = patchRenderResources.unmergedTexture,
        .fbo = unmergedFbo,
        .width = mergeSettings.aabbRes.first,
        .height = mergeSettings.aabbRes.second,
        .imgPath = PREV_METRIC_IMG,
        .glPatches = glPatches,
        .shaderId = patchRenderResources.patchShaderId,
        .aabb = aabb};

    FBtoImg(params);
}

void MergeMetrics::captureAfterMerge(const std::vector<GLfloat> &glPatches, const char *imgPath)
{
    FBtoImgParams params = {
        .texture = patchRenderResources.mergedTexture,
        .fbo = mergedFbo,
        .width = mergeSettings.aabbRes.first,
        .height = mergeSettings.aabbRes.second,
        .imgPath = imgPath,
        .glPatches = glPatches,
        .shaderId = patchRenderResources.patchShaderId,
        .aabb = mergeSettings.aabb};

    FBtoImg(params);
}

float MergeMetrics::getMergeError(const std::vector<GLfloat> &glPatches, const char *imgPath)
{
    switch (mergeSettings.pixelRegion)
    {
    case PixelRegion::Global:
        captureGlobalImage(glPatches, imgPath);
        return evaluateMetric(imgPath, ORIG_IMG);
    case PixelRegion::Local:
        captureAfterMerge(glPatches, imgPath);
        return evaluateMetric(imgPath, PREV_METRIC_IMG);
    }
    return 1.0f;
}

float MergeMetrics::evaluateMetric(const char *compImgPath, const char *compImgPath2)
{
    switch (mergeSettings.metricMode)
    {
    case SSIM:
        return 1.0f - evaluateSSIM(compImgPath2, compImgPath);
    case FLIP:
        return evaluateFLIP(compImgPath2, compImgPath);
    }
    return -1;
}

void saveErrorMapAsPNG(float *errorMap, int width, int height, const char *filename)
{
    // Find the min and max values in the error map to normalize the data
    float minError = FLT_MAX, maxError = -FLT_MAX;
    for (int i = 0; i < width * height; ++i)
    {
        if (errorMap[i] < minError)
            minError = errorMap[i];
        if (errorMap[i] > maxError)
            maxError = errorMap[i];
    }

    // Normalize the error map values to [0, 255] range
    unsigned char *imageData = new unsigned char[width * height]; // Single channel image (grayscale)
    for (int i = 0; i < width * height; ++i)
    {
        float normalizedValue = (errorMap[i] - minError) / (maxError - minError);
        imageData[i] = static_cast<unsigned char>(normalizedValue * 255.0f);
    }

    // Save the image using stb_image_write
    if (!stbi_write_png(filename, width, height, 1, imageData, width))
    {
        std::cerr << "Error writing PNG file." << std::endl;
    }

    delete[] imageData; // Clean up the allocated image data
}

float evaluateFLIP(const char *img1Path, const char *img2Path)
{

    int img1Width, img1Height, img1ChannelCount;
    float *img1 = stbi_loadf(img1Path, &img1Width, &img1Height, &img1ChannelCount, 0);
    if (img1 == NULL)
    {
        fprintf(stderr, "Failed to load image \"%s\": %s\n", img1Path, stbi_failure_reason());
        return -1;
    }

    int img2Width, img2Height, img2ChannelCount;
    float *img2 = stbi_loadf(img2Path, &img2Width, &img2Height, &img2ChannelCount, 0);
    if (img2 == NULL)
    {
        fprintf(stderr, "Failed to load image \"%s\": %s\n", img2Path, stbi_failure_reason());
        return -1;
    }

    if (img1Width != img2Width || img1Height != img2Height || img1ChannelCount != img2ChannelCount)
    {
        fprintf(stderr, "Images must have the same dimensions and number of channels\n");
        return -1;
    }
    float meanFLIPError;
    FLIP::Parameters parameters;
    float *errorMapFLIPOutput = new float[img1Width * img1Height];

    FLIP::evaluate(img1, img2, img1Width, img1Height, false, parameters, false, true, meanFLIPError, &errorMapFLIPOutput);
    saveErrorMapAsPNG(errorMapFLIPOutput, img1Width, img1Height, "img/errormap.png");

    delete[] errorMapFLIPOutput; // If FLIP doesn't manage this internally
    stbi_image_free(img1);
    stbi_image_free(img2);

    return meanFLIPError;
}

float evaluateSSIM(const char *img1Path, const char *img2Path)
{
    int img1Width, img1Height, img1ChannelCount;
    stbi_uc *img1 = stbi_load(img1Path, &img1Width, &img1Height, &img1ChannelCount, 0);
    if (img1 == NULL)
    {
        fprintf(stderr, "Failed to load image \"%s\": %s\n", img1Path, stbi_failure_reason());
        return -1;
    }

    int img2Width, img2Height, img2ChannelCount;
    stbi_uc *img2 = stbi_load(img2Path, &img2Width, &img2Height, &img2ChannelCount, 0);
    if (img2 == NULL)
    {
        fprintf(stderr, "Failed to load image \"%s\": %s\n", img2Path, stbi_failure_reason());
        return -1;
    }

    if (img1Width != img2Width || img1Height != img2Height || img1ChannelCount != img2ChannelCount)
    {
        fprintf(stderr, "Images must have the same dimensions and number of channels\n");
        return -1;
    }

    // Compute SSIM of each channel
    rmgr::ssim::Params params;
    memset(&params, 0, sizeof(params));
    params.width = img1Width;
    params.height = img1Height;
    float totalSSIM = 0;
    for (int channelNum = 0; channelNum < img1ChannelCount; ++channelNum)
    {
        params.imgA.init_interleaved(img1, img1Width * img1ChannelCount, img1ChannelCount, channelNum);
        params.imgB.init_interleaved(img2, img2Width * img2ChannelCount, img2ChannelCount, channelNum);
#if RMGR_SSIM_USE_OPENMP
        const float ssim = rmgr::ssim::compute_ssim_openmp(params);
#else
        const float ssim = rmgr::ssim::compute_ssim(params);
#endif
        if (rmgr::ssim::get_errno(ssim) != 0)
            fprintf(stderr, "Failed to compute SSIM of channel %d\n", channelNum + 1);
        totalSSIM += ssim;
    }

    // Clean up
    stbi_image_free(img2);
    stbi_image_free(img1);
    return totalSSIM * (1.0f / img1ChannelCount);
}

void FBtoImg(const FBtoImgParams &params)
{
    setupFBO(params.texture, params.fbo, params.width, params.height);
    drawPrimitive(params.glPatches, params.shaderId, params.aabb, VERTS_PER_PATCH);
    writeToImage(params.width, params.height, params.imgPath);
    closeFBO();
}

void setupFBO(GLuint texture, GLuint fbo, int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void closeFBO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void writeToImage(int resolution, const char *imgPath)
{
    std::vector<uint8_t> pixelsA(resolution * resolution * 3); // 3 channels for RGB
    glPixelStorei(GL_PACK_ALIGNMENT, 1);                       // very important
    glReadPixels(0, 0, resolution, resolution, GL_RGB, GL_UNSIGNED_BYTE, pixelsA.data());
    int stride_bytes = resolution * 3;
    std::remove(imgPath);
    if (!stbi_write_png(imgPath, resolution, resolution, 3, pixelsA.data(), stride_bytes))
    {
        std::cerr << "Failed to write PNG: " << imgPath << std::endl;
        return;
    }
}

void writeToImage(int width, int height, const char *imgPath)
{
    std::vector<uint8_t> pixelsA(width * height * 3); // 3 channels for RGB
    glPixelStorei(GL_PACK_ALIGNMENT, 1);              // very important
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixelsA.data());
    int stride_bytes = width * 3;
    std::remove(imgPath);
    if (!stbi_write_png(imgPath, width, height, 3, pixelsA.data(), stride_bytes))
    {
        std::cerr << "Failed to write PNG: " << imgPath << std::endl;
        return;
    }
}

GLuint LoadTextureFromFile(const char *filename)
{
    int width, height, channels;
    unsigned char *image_data = stbi_load(filename, &width, &height, &channels, STBI_rgb);
    if (!image_data)
    {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return -1;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(image_data);

    return texture;
}