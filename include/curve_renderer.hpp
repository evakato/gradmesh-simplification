#pragma once

#include "curve.hpp"
#include "gui.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <glad/glad.h>

#include <vector>

class CurveRenderer: public GmsRenderer {
    public:
        CurveRenderer(GmsWindow& window, GmsGui& gui, std::vector<Curve>& curveData);
        ~CurveRenderer();

        const void render();

    protected:
        const void renderCurves();
        const void updateCurveData();

        std::vector<Curve>& curves;
};