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

void MergeMetrics::setEdgeErrorMap(const std::vector<DoubleHalfEdge> &dhes)
{
    edgeErrorPatches = mesh.generatePatches().value();
    edgeErrors = dhes;
    minMaxError.x = std::numeric_limits<float>::max();
    minMaxError.y = std::numeric_limits<float>::min();

    std::vector<float> halfEdgeErrors(mesh.edges.size(), -1.0f);
    for (const auto &dhe : dhes)
    {
        minMaxError.x = std::min(minMaxError.x, dhe.error);
        minMaxError.y = std::max(minMaxError.y, dhe.error);
        halfEdgeErrors[dhe.halfEdgeIdx1] = dhe.error;
        halfEdgeErrors[dhe.halfEdgeIdx2] = dhe.error;
    }
    findMaximumRectangle(halfEdgeErrors);
}

void MergeMetrics::findMaximumRectangle(std::vector<float> &halfEdgeErrors)
{
    auto cornerFaces = mesh.findCornerFaces();
    int currEdgeIdx = cornerFaces[0].first;
    std::vector<int> hist;

    while (true)
    {
        auto currEdge = mesh.edges[currEdgeIdx];
        if (currEdge.twinIdx == -1)
            break;

        if (halfEdgeErrors[currEdgeIdx] < mergeSettings.errorThreshold)
            hist.push_back(1);
        else
            hist.push_back(0);
        currEdgeIdx = mesh.edges[mesh.edges[currEdge.twinIdx].nextIdx].nextIdx;
    }

    hist.push_back(0);
    currEdgeIdx = cornerFaces[0].second;
    bool cols = true;
    int startGridEdgeIdx = cornerFaces[0].second;
    while (true)
    {
        currEdgeIdx = startGridEdgeIdx;
        if (mesh.edges[currEdgeIdx].twinIdx == -1)
            break;

        int end = cols ? hist.size() : hist.size() - 1;
        for (int i = 0; i < end; i++)
        {
            auto currEdge = mesh.edges[currEdgeIdx];

            if (halfEdgeErrors[currEdgeIdx] < mergeSettings.errorThreshold)
                hist[i] += 1;
            else
                hist[i] = 0;

            if (cols)
                currEdgeIdx = mesh.edges[mesh.edges[currEdge.prevIdx].twinIdx].prevIdx;
            else
                currEdgeIdx = mesh.edges[mesh.edges[currEdge.twinIdx].nextIdx].nextIdx;
        }

        if (cols)
        {
            startGridEdgeIdx = mesh.edges[mesh.edges[startGridEdgeIdx].twinIdx].nextIdx;
        }
        else
        {
            startGridEdgeIdx = mesh.edges[startGridEdgeIdx].nextIdx;
        }
        cols = !cols;

        for (int hi : hist)
            std::cout << hi << ", ";
        std::cout << "\n";
    }
}

void MergeMetrics::generateEdgeErrorMap(EdgeErrorDisplay edgeErrorDisplay)
{
    // auto patches = mesh.generatePatches().value();
    for (const auto &dhe : edgeErrors)
    {
        glm::vec3 col;
        switch (edgeErrorDisplay)
        {
        case EdgeErrorDisplay::Binary:
            col = dhe.error >= mergeSettings.errorThreshold ? glm::vec3{1.0f, 0.0f, 0.0f} : black;
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
    for (const auto &be : boundaryEdges)
    {
        edgeErrorPatches[be.patchId].setCurveSelected(be.curveId, blue);
    }
    auto glCurveData = getAllPatchGLData(edgeErrorPatches, &Patch::getCurveData);
    int poolRes = 1000;
    setupFBO(patchRenderResources.unmergedTexture, unmergedFbo, poolRes, poolRes);
    drawPrimitive(glCurveData, patchRenderResources.curveShaderId, mergeSettings.globalAABB, VERTS_PER_CURVE);
    // writeToImage(poolRes, EDGE_MAP_IMG);
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
    // aabb.ensureSize(MIN_AABB_SIZE);
    float wRatio = aabb.width() / mergeSettings.globalPaddedAABB.width();
    float lRatio = aabb.length() / mergeSettings.globalPaddedAABB.length();
    mergeSettings.aabb = aabb;
    // aabb.resizeToSquare();
    mergeSettings.aabbRes = {mergeSettings.globalAABBRes.first * wRatio, mergeSettings.globalAABBRes.second * lRatio};
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