#include "gui.hpp"
#include "gms_app.hpp"

GmsGui::GmsGui(GmsWindow &window, GmsAppState &appState) : gmsWindow(window), appState(appState)
{
    createImguiContext(window.getGLFWwindow());
}

GmsGui::~GmsGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GmsGui::render()
{
    prepareImguiFrame(gmsWindow.getGLFWwindow());

    showRightBar();
    showWindowMenuBar();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GmsGui::showRightBar()
{
    ImGui::SetNextWindowPos(ImVec2(GUI_POS, 0));
    ImGui::SetNextWindowSize(ImVec2(GUI_WIDTH, SCR_HEIGHT));

    ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_NoTitleBar);

    ImGui::Text(("Viewing: " + appState.filename).c_str());
    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::BeginTabBar("Rendering Mode"))
    {
        if (ImGui::BeginTabItem(modeNames[RENDER_PATCHES]))
        {
            appState.currentMode = {RENDER_PATCHES};

            /*
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

                        */
            showHermiteMatrixTable();
            showRenderSettings();

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(modeNames[RENDER_CURVES]))
        {
            appState.currentMode = {RENDER_CURVES};
            /*
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
            */

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Maximum hardware tessellation level: %i", appState.maxHWTessellation);
    // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::End();
}

void GmsGui::showRenderSettings()
{
    ImGui::Checkbox("Wireframe mode", &appState.isWireframeMode);
    ImGui::Checkbox("Draw control points", &appState.renderControlPoints);
    ImGui::Checkbox("Draw handles", &appState.renderHandles);
    if (appState.renderHandles)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Line width", &appState.handleLineWidth, 0.0f, 10.0f);
    }
    ImGui::Checkbox("Draw curves", &appState.renderCurves);
    if (appState.renderCurves)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Curve width", &appState.curveLineWidth, 0.0f, 10.0f);
    }
    ImGui::Checkbox("Draw patches", &appState.renderPatches);
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
            sprintf(label, "%.2f,%.2f", appState.currentPatchData[i].coords.x, appState.currentPatchData[i].coords.y);
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

        glm::vec3 &patchColor = appState.currentPatchData[selected_index].color;
        ImVec4 patchPointColor = ImVec4(patchColor.x, patchColor.y, patchColor.z, 1.00f);

        ImGui::Text("Control point at");
        ImGui::SameLine();
        ImGui::Text(hermiteControlMatrixLabels[selected_index]);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::ColorEdit3("Color", (float *)&patchPointColor, ImGuiColorEditFlags_Float))
        {
            patchColor.x = patchPointColor.x;
            patchColor.y = patchPointColor.y;
            patchColor.z = patchPointColor.z;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
}

void GmsGui::showWindowMenuBar()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(GL_LENGTH, 20));

    ImGui::Begin("Gradient mesh renderer", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
            }
            if (ImGui::MenuItem("Save", "Ctrl+S"))
            {
                saveImage((std::string{IMAGE_DIR} + "/" + extractFileName(appState.filename) + ".png").c_str(), GL_LENGTH, GL_LENGTH);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

static std::string extractFileName(const std::string &filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t lastDot = filepath.find_last_of('.');
    if (lastDot == std::string::npos || lastDot < start)
    {
        return filepath.substr(start);
    }
    return filepath.substr(start, lastDot - start);
}

void createImguiContext(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
}

void prepareImguiFrame(GLFWwindow *window)
{
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }

    ImGui::GetIO().FontGlobalScale = 1.0f;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::StyleColorsLight();
}