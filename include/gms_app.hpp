#pragma once

#include "curve_renderer.hpp"
#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gui.hpp"
#include "merging.hpp"
#include "patch_renderer.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <GLFW/glfw3.h>

class GmsApp
{
public:
    GmsApp();
    ~GmsApp() {}
    void run();

private:
    void loadMesh();
    void setupNewMesh();

    GmsWindow gmsWindow{SCR_WIDTH, SCR_HEIGHT, "gms"};
    GmsGui gui{gmsWindow, appState};
    CurveRenderer curveRenderer{gmsWindow, appState};
    PatchRenderer patchRenderer{gmsWindow, appState};
    GradMeshMerger merger{appState};
    GmsAppState appState{};
};