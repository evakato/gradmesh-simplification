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
    PatchRenderer(GmsWindow &window, GmsAppState &appState);
    ~PatchRenderer();

    void render(std::vector<GLfloat> &glPatches, std::vector<Patch> &patches, std::vector<Vertex> &handles);
    int getPatchShaderId() const { return patchShaderId; }

protected:
    void renderPatches(std::vector<GLfloat> &glPatches, std::vector<Patch> &patches, std::vector<Vertex> &handles);
    void updatePatchData(std::vector<Patch> &patches);

    GLuint EBO;
    GLuint patchShaderId;
};