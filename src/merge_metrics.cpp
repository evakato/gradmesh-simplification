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

void MergeMetrics::captureBeforeMerge(int halfEdgeIdx, std::vector<GLfloat> &glPatches)
{
    AABB aabb;
    switch (mergeSettings.pixelRegion)
    {
    case PixelRegion::Local:
    {
        auto [face1RIdx, face1BIdx, face1LIdx, face1TIdx] = mesh.getFaceEdgeIdxs(halfEdgeIdx);
        auto [face2LIdx, face2TIdx, face2RIdx, face2BIdx] = mesh.getFaceEdgeIdxs(mesh.getTwinIdx(halfEdgeIdx));
        int twin1 = mesh.getTwinIdx(face1BIdx);
        int twin2 = mesh.getTwinIdx(face1TIdx);
        int twin3 = mesh.getTwinIdx(face2BIdx);
        int twin4 = mesh.getTwinIdx(face2TIdx);
        aabb = mesh.getBoundingBoxOverFaces({halfEdgeIdx, face2LIdx, twin1, twin2, twin3, twin4});
        break;
    }
    case PixelRegion::Global:
    {
        aabb = mesh.getAABB();
        break;
    }
    }
    aabb.addPadding(mergeSettings.aabbPadding);
    aabb.ensureSize(MIN_AABB_SIZE);
    aabb.resizeToSquare();

    FBOParams params{ORIG_METRIC_IMG, patchRenderResources.unmergedTexture, unmergedFbo, glPatches, patchRenderResources.patchShaderId, aabb, mergeSettings.poolRes};
    renderFBO(params);
    mergeSettings.aabb = aabb;
}

bool MergeMetrics::doMerge(const std::vector<GLfloat> &glPatches)
{
    FBOParams params{MERGE_METRIC_IMG, patchRenderResources.mergedTexture, mergedFbo, glPatches, patchRenderResources.patchShaderId, mergeSettings.aabb, mergeSettings.poolRes};
    // renderFBO(MERGE_METRIC_IMG, patchRenderResources.mergedTexture, mergedFbo, glPatches, patchRenderResources.patchShaderId, mergeSettings.aabb);
    renderFBO(params);
    float error;
    switch (mergeSettings.metricMode)
    {
    case SSIM:
    {
        error = 1.0f - evaluateSSIM(ORIG_METRIC_IMG, MERGE_METRIC_IMG);
        break;
    }
    case FLIP:
    {
        error = evaluateFLIP(ORIG_METRIC_IMG, MERGE_METRIC_IMG);
        break;
    }
    }

    return error < mergeSettings.errorThreshold;
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

void renderFBO(const FBOParams &params)
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBufferData(GL_ARRAY_BUFFER, params.glPatches.size() * sizeof(GLfloat), params.glPatches.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);

    glUseProgram(params.patchShaderId);
    setAABBProjMat(params.patchShaderId, params.aabb);
    glLineWidth(1.0f);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
    for (int i = 0; i < params.glPatches.size(); i++)
        glDrawArrays(GL_PATCHES, i * VERTS_PER_PATCH, VERTS_PER_PATCH);

    std::vector<uint8_t> pixelsA(params.resolution * params.resolution * 3); // 3 channels for RGB
    glReadPixels(0, 0, params.resolution, params.resolution, GL_RGB, GL_UNSIGNED_BYTE, pixelsA.data());
    int stride_bytes = params.resolution * 3;
    stbi_write_png(params.imgPath, params.resolution, params.resolution, 3, pixelsA.data(), stride_bytes);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}