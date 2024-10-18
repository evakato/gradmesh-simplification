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

    void render(std::vector<Patch> &patches, std::vector<Vertex> &handles);
    void bindBuffers();
    int getPatchShaderId() const { return patchShaderId; }
    std::vector<GLfloat> &getGlPatches() { return glPatches; }

protected:
    void renderPatches(std::vector<Patch> &patches, std::vector<Vertex> &handles);
    void updatePatchData(std::vector<Patch> &patches);

    GLuint EBO;
    GLuint patchShaderId;

private:
    std::vector<GLfloat> glPatches;
};