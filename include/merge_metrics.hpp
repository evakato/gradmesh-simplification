#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unordered_map>

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

inline const int POOLING_LENGTH{700};
inline const float AABB_PADDING{0.03f};
inline const glm::vec2 MIN_AABB_SIZE{0.1f, 0.1f};
inline const float ERROR_THRESHOLD{0.005f};

enum class EdgeErrorDisplay
{
    Binary,
    Normalized,
    Scaled
};

struct ValenceVertex
{
    int id;                          // point idx
    std::array<int, 4> halfEdgeIdxs; // 4 half edges that stem from it
    std::array<int, 4> markedEdges;  // 0 for unmarked, 1 for marked
    int isCorner() const
    {
        int count = 0;
        for (const int edgeIdx : halfEdgeIdxs)
            if (edgeIdx == -1)
                count++;
        if (count < 3)
            return 0;
        for (const int edgeIdx : halfEdgeIdxs)
            if (edgeIdx != -1)
                return edgeIdx;
        return 0;
    }
    bool isBoundary() const
    {
        int count = 0;
        for (const int edgeIdx : halfEdgeIdxs)
            if (edgeIdx == -1)
                count++;
        return count >= 2;
    }
    int sumMarked() const
    {
        return markedEdges[0] + markedEdges[1] + markedEdges[2] + markedEdges[3];
    }
    bool noGrow() const
    {
        return (isBoundary() || sumMarked() == 0 || sumMarked() == 3 || sumMarked() == 4);
    }
    bool isDone() const
    {
        return (isBoundary() || (sumMarked() != 1 && valenceTwoL().first == -1));
    }
    int getFirstMarkedEdgeIndex() const
    {
        auto it = std::find(markedEdges.begin(), markedEdges.end(), 1);
        if (it != markedEdges.end())
        {
            return static_cast<int>(std::distance(markedEdges.begin(), it));
        }
        return -1;
    }
    int valenceOne() const
    {
        if (sumMarked() != 1)
            return -1;
        if (halfEdgeIdxs[(getFirstMarkedEdgeIndex() + 2) % 4] == -1)
            return -1;
        return (getFirstMarkedEdgeIndex() + 2) % 4;
        // return halfEdgeIdxs[(getFirstMarkedEdgeIndex() + 2) % 4];
    }
    std::pair<int, int> valenceTwoL() const
    {
        if (sumMarked() != 2)
            return {-1, -1};
        if ((markedEdges[0] && markedEdges[2]) || (markedEdges[1] && markedEdges[3]))
            return {-1, -1};

        if (markedEdges[0] && markedEdges[1])
            return {2, 3};
        if (markedEdges[1] && markedEdges[2])
            return {0, 3};
        if (markedEdges[2] && markedEdges[3])
            return {0, 1};
        if (markedEdges[0] && markedEdges[3])
            return {1, 2};
        return {-1, -1};
    }
    void setMarked(int i)
    {
        markedEdges[i] = 1;
    }
    void unmark(int i)
    {
        markedEdges[i] = 0;
    }
};

struct MergeableRegion
{
    std::pair<int, int> gridPair;
    std::pair<int, int> maxRegion;
    float sumOfErrors;
};

struct SingleHalfEdge
{
    CurveId curveId;
    int halfEdgeIdx;
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
        float singleMergeErrorThreshold{0.0001f};
        bool showMotorcycleEdges = true;
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
    void setBoundaryEdges(std::vector<SingleHalfEdge> &bes) { boundaryEdges = bes; }
    float evaluateMetric(const char *compImgPath = MERGE_METRIC_IMG, const char *compImgPath2 = ORIG_IMG);
    void setValenceVertices();
    std::vector<MergeableRegion> getMergeableRegions();
    void findSumOfErrors(MergeableRegion &mr);

private:
    void generateMotorcycleGraph();
    void markTwoHalfEdges(int idx1, int idx2);
    void unmarkTwoHalfEdges(int idx1, int idx2);
    bool isMarked(int halfEdgeIdx);
    void getMergeableRegion(std::vector<int> &alreadyVisited, std::vector<MergeableRegion> &mergeableRegions, int halfEdgeIdx);

    GLuint unmergedFbo;
    GLuint mergedFbo;

    GradMesh &mesh;
    PatchRenderResources &patchRenderResources;
    MergeSettings &mergeSettings;

    std::vector<Patch> edgeErrorPatches;
    std::vector<DoubleHalfEdge> edgeErrors;
    glm::vec2 minMaxError;
    std::vector<SingleHalfEdge> boundaryEdges;
    std::vector<ValenceVertex> valenceVertices;
    std::vector<DoubleHalfEdge> motorcycleEdges;
    std::vector<float> halfEdgeErrors;

    std::unordered_map<int, std::pair<int, int>> lookupValenceVertex;
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