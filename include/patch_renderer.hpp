#pragma once

#include "gui.hpp"
#include "patch.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class PatchRenderer : public GmsRenderer
{
public:
    PatchRenderer(GmsWindow &window, GmsGui &gui, std::vector<Patch> &patchData);
    ~PatchRenderer();

    void render();

protected:
    void renderPatches();
    void updatePatchData();
    void setupShaders();

    GLuint EBO;
    GLuint patchShaderId;

    // TODO: create a base class to inherit patch and curve rendering separately
    std::vector<Patch> &patches;
};