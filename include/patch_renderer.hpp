#pragma once

#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class PatchRenderer : public GmsRenderer
{
public:
    PatchRenderer(GmsWindow &window, GmsAppState &appState, std::vector<Patch> &patchData);
    ~PatchRenderer();

    void render();
    void bindBuffers();

protected:
    void renderPatches();
    void updatePatchData();
    void setupShaders();

    GLuint EBO;
    GLuint patchShaderId;

    std::vector<Patch> &patches;
};