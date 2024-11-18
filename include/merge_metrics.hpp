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
#include "types.hpp"

inline const int POOLING_LENGTH{500};
inline const float AABB_PADDING{0.03f};
inline const glm::vec2 MIN_AABB_SIZE{0.1f, 0.1f};

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
        AABB aabb{glm::vec2(0.0f), glm::vec2(0.0f)};
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
    void captureBeforeMerge(std::vector<GLfloat> &glPatches, int halfEdgeIdx = -1);
    void captureGlobalImage(std::vector<GLfloat> &glPatches, const char *imgPath);
    void captureAfterMerge(const std::vector<GLfloat> &glPatches, const char *imgPath);
    float getMergeError(const char *compImgPath = MERGE_METRIC_IMG, const char *compImgPath2 = ORIG_IMG);
    void setAABB()
    {
        globalAABB = mesh.getAABB();
        globalAABB.addPadding(mergeSettings.aabbPadding);
        globalAABB.ensureSize(MIN_AABB_SIZE);
        globalAABB.resizeToSquare();
        mergeSettings.aabb = globalAABB;
    }
    void setEdgeErrorMap(const std::vector<DoubleHalfEdge> &dhes);
    void generateEdgeErrorMap(EdgeErrorDisplay edgeErrorDisplay);
    void setBoundaryEdges(std::vector<CurveId> &bes) { boundaryEdges = bes; }

private:
    void findMaximumRectangle(std::vector<float> &halfEdgeErrors);

    GLuint unmergedFbo;
    GLuint mergedFbo;
    AABB globalAABB;

    GradMesh &mesh;
    PatchRenderResources &patchRenderResources;
    MergeSettings &mergeSettings;

    std::vector<Patch> edgeErrorPatches;
    std::vector<DoubleHalfEdge> edgeErrors;
    glm::vec2 minMaxError;
    std::vector<CurveId> boundaryEdges;
};

struct SetupFBOParams
{
    GLuint texture;
    GLuint fbo;
    int resolution;
};

float evaluateSSIM(const char *img1Path, const char *img2Path);
float evaluateFLIP(const char *img1Path, const char *img2Path);
void setAABBProjMat(int shaderId, AABB aabb);
void drawPatches(const std::vector<GLfloat> &glPatches, int patchShaderId, const AABB &aabb);
void writeToImage(int resolution, const char *imgPath);
void closeFBO();
void setupFBO(const SetupFBOParams &params);
void drawCurves(const std::vector<GLfloat> &glCurves, int curveShaderId, const AABB &aabb);
void drawPatches(const std::vector<GLfloat> &glPatches, int patchShaderId, const AABB &aabb);

GLuint LoadTextureFromFile(const char *filename);