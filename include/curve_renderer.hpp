#pragma once

#include "curve.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <glad/glad.h>

#include <vector>

class CurveRenderer : public GmsRenderer
{
public:
    enum CurveMode
    {
        Bezier,
        Hermite
    };
    struct CurveRenderParams
    {
        CurveMode curveMode;
        std::vector<Curve> curves;
    };
    CurveRenderer(GmsWindow &window, GmsAppState &appState);
    ~CurveRenderer();

    void render();

private:
    void renderCurves();
    void updateCurveData();
};