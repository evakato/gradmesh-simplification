#include "gui.hpp"

GmsGui::GmsGui(GLFWwindow *window) : window(window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
}

GmsGui::~GmsGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GmsGui::createGUIFrame()
{
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }

    ImGui::GetIO().FontGlobalScale = 1.0f;
    ImGui::SetNextWindowSize(ImVec2(0.0, 0.0), ImGuiCond_Always);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    {

        ImGui::SetNextWindowPos(ImVec2(GUI_POS, 0));
        ImGui::SetNextWindowSize(ImVec2(GUI_WIDTH, SCR_HEIGHT));

        ImGui::Begin("Hello, world!");

        if (ImGui::BeginTabBar("Rendering Mode"))
        {
            if (ImGui::BeginTabItem(modeNames[RENDER_PATCHES]))
            {
                currentMode = {RENDER_PATCHES};

                if (GmsWindow::validSelectedPoint())
                {
                    ImGui::Text("Selected point: (%i, %i)", GmsWindow::selectedPoint.primitiveId, GmsWindow::selectedPoint.pointId);
                    ImGui::SameLine();
                    ImGui::Text("at (%.3f, %.3f)", GmsWindow::mousePos.x, GmsWindow::mousePos.y);
                }
                else
                {
                    ImGui::Text("Selected point: none");
                }

                showHermiteMatrixTable();
                showRenderSettings();

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(modeNames[RENDER_CURVES]))
            {
                currentMode = {RENDER_CURVES};
                imCurrentColor = ImVec4(currentColor.x, currentColor.y, currentColor.z, 1.00f);
                currentColorChanged = ImGui::ColorEdit3("Current color", (float *)&imCurrentColor);
                ImGui::SliderFloat("Curve width", &lineWidth, 0.0f, 10.0f);

                if (GmsWindow::validSelectedPoint())
                {
                    ImGui::Text("Selected point: (%i, %i)", GmsWindow::selectedPoint.primitiveId, GmsWindow::selectedPoint.pointId);
                    ImGui::SameLine();
                    ImGui::Text("at (%.3f, %.3f)", GmsWindow::mousePos.x, GmsWindow::mousePos.y);
                }
                else
                {
                    ImGui::Text("Selected point: none");
                }

                ImGui::EndTabItem();
            }
            selectedTab = modeNames[currentMode];
            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Maximum hardware tessellation level: %i", maxHWTessellation);
        // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

        ImGui::End();
    }
}

void GmsGui::renderGUI()
{
    createGUIFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GmsGui::showRenderSettings()
{
    ImGui::Checkbox("Wireframe mode", &isWireframeMode);

    ImGui::Checkbox("Draw control points", &drawControlPoints);

    ImGui::Checkbox("Draw handles", &drawHandles);
    if (drawHandles)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Line width", &handleLineWidth, 0.0f, 10.0f);
    }

    ImGui::Checkbox("Draw curves", &drawCurves);
    if (drawCurves)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Curve width", &curveLineWidth, 0.0f, 10.0f);
    }

    ImGui::Checkbox("Draw patches", &drawPatches);
}

void GmsGui::showHermiteMatrixTable()
{
    static int selected_index = -1; // Stores the index of the currently selected item

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Hermite control matrix:");
    if (ImGui::BeginTable("split1", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
    {
        for (int i = 0; i < 16; i++)
        {
            char label[32];
            sprintf(label, "%.2f,%.2f", currentPatchData[i].coords.x, currentPatchData[i].coords.y);
            ImGui::TableNextColumn();

            bool is_selected = (selected_index == i);
            ImGui::PushID(i);
            if (ImGui::Selectable(label, is_selected))
            {
                selected_index = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (selected_index != -1)
    {
        ImGui::Spacing();

        glm::vec3 &patchColor = currentPatchData[selected_index].color;
        ImVec4 patchPointColor = ImVec4(patchColor.x, patchColor.y, patchColor.z, 1.00f);

        ImGui::Text("Control point at");
        ImGui::SameLine();
        ImGui::Text(hermiteControlMatrixLabels[selected_index]);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::ColorEdit3("Color", (float *)&patchPointColor, ImGuiColorEditFlags_Float))
        {
            patchColorChange = true;
            patchColor.x = patchPointColor.x;
            patchColor.y = patchPointColor.y;
            patchColor.z = patchPointColor.z;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
}