#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <rmgr/ssim.h>
#include <rmgr/ssim-openmp.h>
#include <FLIP.h>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image_write.h"

#include "gradmesh.hpp"
#include "types.hpp"

inline const int POOLING_LENGTH{500};
inline const float AABB_PADDING{0.03f};
inline const glm::vec2 MIN_AABB_SIZE{0.1f, 0.1f};

// Facilitates the GradMeshMerger class in evaluating pixel-based metrics for a given merge
class MergeMetrics
{
public:
    enum MetricMode
    {
        SSIM,
        FLIP
    };
    enum PixelRegion
    {
        Global,
        Local
    };
    struct PatchRenderResources
    {
        GLuint unmergedTexture;
        GLuint mergedTexture;
        int patchShaderId;
    };
    struct MergeSettings
    {
        MetricMode metricMode = MetricMode::FLIP;
        PixelRegion pixelRegion = PixelRegion::Global;
        AABB aabb;
        float errorThreshold = ERROR_THRESHOLD;
        int poolRes = POOLING_LENGTH;
        float aabbPadding = AABB_PADDING;
    };
    struct Params
    {
        GradMesh &mesh;
        MergeSettings &mergeSettings;
        PatchRenderResources &patchRenderResources;
    };

    MergeMetrics(Params params);
    void captureBeforeMerge(int halfEdgeIdx, std::vector<GLfloat> &glPatches);
    bool doMerge(const std::vector<GLfloat> &glPatches);

private:
    GLuint unmergedFbo;
    GLuint mergedFbo;

    GradMesh &mesh;
    PatchRenderResources &patchRenderResources;
    MergeSettings &mergeSettings;
};

struct FBOParams
{
    const char *imgPath;
    GLuint texture;
    GLuint fbo;
    const std::vector<GLfloat> &glPatches;
    int patchShaderId;
    AABB aabb;
    int resolution;
};
float evaluateSSIM(const char *img1Path, const char *img2Path);
float evaluateFLIP(const char *img1Path, const char *img2Path);
void renderFBO(const FBOParams &params);
void setAABBProjMat(int shaderId, AABB aabb);