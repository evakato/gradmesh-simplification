#pragma once

#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "rmgr/ssim.h"
#include "merge_metrics.hpp"

#include <vector>

class PatchRenderer : public GmsRenderer
{
public:
    struct PatchRenderParams
    {
        std::vector<GLfloat> glPatches;
        std::vector<GLfloat> glCurves;
        std::vector<Vertex> handles;
        std::vector<Vertex> points;
    };
    PatchRenderer(GmsWindow &window, GmsAppState &appState);
    ~PatchRenderer();

    void render(PatchRenderParams &params);

protected:
    void renderPatches(PatchRenderParams &params);
    // modify patches vector based on user input
    void updatePatches(std::vector<Patch> &patches);

private:
    void resetPreviousSelection();

    GLuint EBO;
};