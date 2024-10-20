#include "merge_metrics.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

MergeMetrics::MergeMetrics(int patchShaderId) : patchShaderId{patchShaderId} {}

bool MergeMetrics::doMerge(std::vector<GLfloat> &glPatches, AABB aabb, float eps)
{
    renderFBO(MERGE_METRIC_IMG, glPatches, patchShaderId, aabb);
    float error = evaluateSSIM(ORIG_METRIC_IMG, MERGE_METRIC_IMG);
    return (1.0f - error) < eps;
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
        const float ssim = rmgr::ssim::compute_ssim(params);
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

void renderFBO(const char *imgPath, std::vector<GLfloat> &glPatches, int patchShaderId, AABB aabb)
{
    GLuint fbo, texture;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_LENGTH, GL_LENGTH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, GL_LENGTH, GL_LENGTH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBufferData(GL_ARRAY_BUFFER, glPatches.size() * sizeof(GLfloat), glPatches.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);

    glUseProgram(patchShaderId);
    setAABBProjMat(patchShaderId, aabb);
    glLineWidth(1.0f);
    glPatchParameteri(GL_PATCH_VERTICES, VERTS_PER_PATCH);
    for (int i = 0; i < glPatches.size(); i++)
        glDrawArrays(GL_PATCHES, i * VERTS_PER_PATCH, VERTS_PER_PATCH);

    std::vector<uint8_t> pixelsA(GL_LENGTH * GL_LENGTH * 3); // 3 channels for RGB
    glReadPixels(0, 0, GL_LENGTH, GL_LENGTH, GL_RGB, GL_UNSIGNED_BYTE, pixelsA.data());
    stbi_write_png(imgPath, GL_LENGTH, GL_LENGTH, 3, pixelsA.data(), GL_LENGTH * 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}