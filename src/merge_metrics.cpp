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
    auto cornerFaces = mesh.findCornerFace();
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
    SetupFBOParams params{patchRenderResources.unmergedTexture, unmergedFbo, poolRes};
    setupFBO(params);
    drawCurves(glCurveData, patchRenderResources.curveShaderId, globalAABB);
    // writeToImage(poolRes, EDGE_MAP_IMG);
    closeFBO();
}

void MergeMetrics::captureGlobalImage(std::vector<GLfloat> &glPatches, const char *imgPath)
{
    SetupFBOParams params{patchRenderResources.unmergedTexture, unmergedFbo, mergeSettings.poolRes};
    setupFBO(params);
    drawPatches(glPatches, patchRenderResources.patchShaderId, globalAABB);
    writeToImage(mergeSettings.poolRes, imgPath);
    closeFBO();
}

void MergeMetrics::captureBeforeMerge(std::vector<GLfloat> &glPatches, int halfEdgeIdx)
{
    if (mergeSettings.pixelRegion == PixelRegion::Global)
        return;

    AABB aabb;
    auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = mesh.getFaceEdgeIdxs(halfEdgeIdx);
    auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = mesh.getFaceEdgeIdxs(mesh.getTwinIdx(halfEdgeIdx));
    int twin1 = mesh.getTwinIdx(face1BIdx);
    int twin2 = mesh.getTwinIdx(face1TIdx);
    int twin3 = mesh.getTwinIdx(face2BIdx);
    int twin4 = mesh.getTwinIdx(face2TIdx);
    aabb = mesh.getBoundingBoxOverFaces({halfEdgeIdx, face2LIdx, twin1, twin2, twin3, twin4});
    aabb.addPadding(mergeSettings.aabbPadding);
    aabb.ensureSize(MIN_AABB_SIZE);
    aabb.resizeToSquare();

    SetupFBOParams params{patchRenderResources.unmergedTexture, unmergedFbo, mergeSettings.poolRes};
    setupFBO(params);
    drawPatches(glPatches, patchRenderResources.patchShaderId, aabb);
    writeToImage(mergeSettings.poolRes, PREV_METRIC_IMG);
    closeFBO();
    mergeSettings.aabb = aabb;
}

void MergeMetrics::captureAfterMerge(const std::vector<GLfloat> &glPatches, const char *imgPath)
{
    SetupFBOParams params{patchRenderResources.mergedTexture, mergedFbo, mergeSettings.poolRes};
    setupFBO(params);
    drawPatches(glPatches, patchRenderResources.patchShaderId, mergeSettings.aabb);
    writeToImage(mergeSettings.poolRes, imgPath);
    closeFBO();
}

float MergeMetrics::getMergeError(const char *compImgPath, const char *compImgPath2)
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

void setAABBProjMat(int shaderId, AABB aabb)
{
    glm::mat4 projMat = glm::mat4(1.0f);
    projMat = glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, -1.0f, 1.0f);
    GLint projectionLoc = glGetUniformLocation(shaderId, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projMat[0][0]);
}

void closeFBO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setupFBO(const SetupFBOParams &params)
{
    glBindFramebuffer(GL_FRAMEBUFFER, params.fbo);
    glBindTexture(GL_TEXTURE_2D, params.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, params.resolution, params.resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, params.texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer is not complete!" << std::endl;

    glViewport(0, 0, params.resolution, params.resolution);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void writeToImage(int resolution, const char *imgPath)
{
    std::vector<uint8_t> pixelsA(resolution * resolution * 3); // 3 channels for RGB
    glReadPixels(0, 0, resolution, resolution, GL_RGB, GL_UNSIGNED_BYTE, pixelsA.data());
    int stride_bytes = resolution * 3;
    stbi_write_png(imgPath, resolution, resolution, 3, pixelsA.data(), stride_bytes);
}

void drawCurves(const std::vector<GLfloat> &glCurves, int curveShaderId, const AABB &aabb)
{

    glBufferData(GL_ARRAY_BUFFER, glCurves.size() * sizeof(GLfloat), glCurves.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);
    glUseProgram(curveShaderId);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_CURVE);
    setAABBProjMat(curveShaderId, aabb);
    glDrawArrays(GL_PATCHES, 0, glCurves.size() / 5);
}

void drawPatches(const std::vector<GLfloat> &glPatches, int patchShaderId, const AABB &aabb)
{
    glBufferData(GL_ARRAY_BUFFER, glPatches.size() * sizeof(GLfloat), glPatches.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);

    glUseProgram(patchShaderId);
    setAABBProjMat(patchShaderId, aabb);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
    for (int i = 0; i < glPatches.size(); i++)
        glDrawArrays(GL_PATCHES, i * VERTS_PER_PATCH, VERTS_PER_PATCH);
}

GLuint LoadTextureFromFile(const char *filename)
{
    // Load the image using stb_image with RGB format (without alpha channel)
    int width, height, channels;
    unsigned char *image_data = stbi_load(filename, &width, &height, &channels, STBI_rgb); // Load as RGB

    if (!image_data)
    {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return 0;
    }

    // Generate OpenGL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Upload the image data to OpenGL (as RGB)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);

    // Set texture parameters (optional, but recommended)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Free the image data as we no longer need it
    stbi_image_free(image_data);

    return texture;
}