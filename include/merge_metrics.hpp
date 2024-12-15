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
#include "patch.hpp"
#include "renderer.hpp"
#include "types.hpp"

inline const int POOLING_LENGTH{400};
inline const float AABB_PADDING{0.03f};
inline const glm::vec2 MIN_AABB_SIZE{0.1f, 0.1f};
inline const float ERROR_THRESHOLD{0.0075f};

enum class EdgeErrorDisplay
{
    Binary,
    Normalized,
    Scaled
};

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
        int curveShaderId;
    };
    struct MergeSettings
    {
        MetricMode metricMode = MetricMode::SSIM;
        PixelRegion pixelRegion = PixelRegion::Global;
        AABB aabb{};
        std::pair<int, int> aabbRes;
        AABB globalAABB{};
        AABB globalPaddedAABB{};
        std::pair<int, int> globalAABBRes;
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
    void setAABB();
    void captureGlobalImage(const std::vector<GLfloat> &glPatches, const char *imgPath);
    void captureBeforeMerge(const std::vector<GLfloat> &glPatches, AABB &aabb);
    void captureAfterMerge(const std::vector<GLfloat> &glPatches, const char *imgPath);
    float getMergeError(const std::vector<GLfloat> &glPatches, const char *imgPath);

    void setEdgeErrorMap(const std::vector<DoubleHalfEdge> &dhes);
    void generateEdgeErrorMap(EdgeErrorDisplay edgeErrorDisplay);
    void setBoundaryEdges(std::vector<CurveId> &bes) { boundaryEdges = bes; }
    float evaluateMetric(const char *compImgPath = MERGE_METRIC_IMG, const char *compImgPath2 = ORIG_IMG);

private:
    void findMaximumRectangle(std::vector<float> &halfEdgeErrors);

    GLuint unmergedFbo;
    GLuint mergedFbo;

    GradMesh &mesh;
    PatchRenderResources &patchRenderResources;
    MergeSettings &mergeSettings;

    std::vector<Patch> edgeErrorPatches;
    std::vector<DoubleHalfEdge> edgeErrors;
    glm::vec2 minMaxError;
    std::vector<CurveId> boundaryEdges;
};

struct FBtoImgParams
{
    GLuint texture;
    GLuint fbo;
    int width;
    int height;
    const char *imgPath;
    const std::vector<GLfloat> &glPatches;
    int shaderId;
    const AABB &aabb;
};

float evaluateSSIM(const char *img1Path, const char *img2Path);
float evaluateFLIP(const char *img1Path, const char *img2Path);
void drawPatches(const std::vector<GLfloat> &glPatches, int patchShaderId, const AABB &aabb);
void writeToImage(int resolution, const char *imgPath);
void writeToImage(int width, int height, const char *imgPath);

void setupFBO(GLuint texture, GLuint fbo, int width, int height);
void closeFBO();

void FBtoImg(const FBtoImgParams &params);

GLuint LoadTextureFromFile(const char *filename);