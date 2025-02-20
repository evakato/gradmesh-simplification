#pragma once

#include "curve_renderer.hpp"
#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gui.hpp"
#include "merging.hpp"
#include "preprocessing.hpp"
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
    void runTest(MergeSelectMode mode);

    GmsWindow gmsWindow{SCR_WIDTH, SCR_HEIGHT, "gms"};
    GmsGui gui{gmsWindow, appState};
    CurveRenderer curveRenderer{gmsWindow, appState};
    PatchRenderer patchRenderer{gmsWindow, appState};
    GradMeshMerger merger{appState};
    MergePreprocessor preprocessor{merger, appState};
    GmsAppState appState{};
};