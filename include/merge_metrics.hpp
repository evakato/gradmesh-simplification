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
    struct Params
    {
        int patchShaderId;
        std::vector<GLfloat> &unmergedGlPatches;
        GLuint &unmergedTexture;
        GLuint &mergedTexture;
        GradMesh &mesh;
        AABB &aabb;
    };

    MergeMetrics(Params params);
    void setAABB(PixelRegion region, int halfEdgeIdx);
    bool doMerge(std::vector<GLfloat> &glPatches, MetricMode metric, float eps);
    void captureBeforeMerge() const;

private:
    GLuint unmergedFbo;
    GLuint mergedFbo;
    int patchShaderId;
    std::vector<GLfloat> &unmergedGlPatches;

    GLuint &unmergedTexture;
    GLuint &mergedTexture;
    GradMesh &mesh;
    AABB &aabb;
};

float evaluateSSIM(const char *img1Path, const char *img2Path);
float evaluateFLIP(const char *img1Path, const char *img2Path);
void renderFBO(const char *imgPath, GLuint texture, GLuint fbo, std::vector<GLfloat> &glPatches, int patchShaderId, AABB aabb);
void setAABBProjMat(int shaderId, AABB aabb);