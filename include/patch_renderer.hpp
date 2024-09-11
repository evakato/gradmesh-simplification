#pragma once

#include "gui.hpp"
#include "patch.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <glad/glad.h>

#include <vector>

class PatchRenderer: public GmsRenderer {
    public:
        PatchRenderer(GmsWindow& window, GmsGui& gui, std::vector<Patch>& patchData);
        ~PatchRenderer();

        const void render();

    protected:
        const void renderPatches();
        const void updatePatchData();
        const void setupShaders();

        GLuint EBO;
        GLuint gradMeshShaderId;

        // TODO: create a base class to inherit patch and curve rendering separately
        std::vector<Patch>& patches;
};