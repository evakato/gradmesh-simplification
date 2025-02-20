#include "gui.hpp"

ImFont *iconFont;
ImFont *mainFont;
static bool showFinalImages = false;
static int selectedHalfEdgeIdx = -1;
static int fileDialogMode = -1; // 0 = mesh filename, 1 = preprocessing
static bool showManualMerge = false;
static ImVec2 manualMergeWindow;

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
    renderComparisonWindow(appState);
    showEdgeErrorMap(appState);
    showConflictGraphStats(appState);

    processKeyInput(gmsWindow.getGLFWwindow(), appState, fileDialog);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void renderComparisonWindow(const GmsAppState &appState)
{
    if (showFinalImages && (appState.numOfMerges > 0 || appState.regionsMerged > 0))
    {
        // Set position and size for the new window
        ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_Once);     // Position of the new window
        ImGui::SetNextWindowSize(ImVec2(1220, 700), ImGuiCond_Once); // Size of the new window

        if (ImGui::Begin("Image comparison", &showFinalImages))
        {
            if (ImGui::BeginTable("ImageTable2", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Original Image", ImGuiTableColumnFlags_WidthFixed, GUI_FINAL_IMAGE_SIZE);
                ImGui::TableSetupColumn("Merged Image", ImGuiTableColumnFlags_WidthFixed, GUI_FINAL_IMAGE_SIZE);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow(ImGuiTableRowFlags_None, GUI_FINAL_IMAGE_SIZE + 10);
                ImGui::TableSetColumnIndex(0);
                GLuint texture = LoadTextureFromFile(ORIG_IMG);
                auto [w, h] = appState.mergeSettings.globalPaddedAABB.getRes(GUI_FINAL_IMAGE_SIZE);
                ImGui::Image((ImTextureID)texture, ImVec2(w, h));

                ImGui::TableSetColumnIndex(1);
                GLuint texture2 = LoadTextureFromFile(CURR_IMG);
                ImGui::Image((ImTextureID)texture2, ImVec2(w, h));
                ImGui::EndTable();
            }
        }
        ImGui::End(); // End the new window
    }
}

void showConflictGraphStats(GmsAppState &appState)
{
    if (appState.mergeProcess == MergeProcess::ViewConflictGraphStats)
    {
        ImGui::SetNextWindowPos(ImVec2(80, 95), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(250, 165), ImGuiCond_Once);
        bool showConflictGraphStats = true;
        if (ImGui::Begin("Conflict graph stats", &showConflictGraphStats))
        {
            ImGui::Text("Nodes: %d", appState.conflictGraphStats.numOfNodes);
            ImGui::Text("Average degree: %.4f", appState.conflictGraphStats.avgDegree);
            ImGui::Text("Average quads: %.4f", appState.conflictGraphStats.avgQuads);
            ImGui::Text("Average error: %.4f", appState.conflictGraphStats.avgError);
            if (!showConflictGraphStats)
            {
                appState.mergeProcess = MergeProcess::Merging;
            }
        }
        ImGui::End();
    }
}

void showEdgeErrorMap(GmsAppState &appState)
{
    if (appState.mergeProcess == MergeProcess::ViewEdgeMap)
    {
        ImGui::SetNextWindowPos(ImVec2(75, 70), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(950, 800), ImGuiCond_Once);

        bool showErrorMap = true;
        if (ImGui::Begin("Merge edge error map", &showErrorMap))
        {
            auto [w, h] = appState.mergeSettings.globalPaddedAABB.getRes(GUI_ERROR_MAP_SIZE);
            // ImGui::Image((void *)(intptr_t)appState.patchRenderResources.unmergedTexture, ImVec2(w, h));
            ImGui::Image((ImTextureID)appState.patchRenderResources.unmergedTexture, ImVec2(w, h));

            ImGui::SameLine();

            ImGui::BeginGroup();
            {
                EdgeErrorDisplay &displayMode = appState.edgeErrorDisplay;

                if (ImGui::RadioButton("Normalized", (int *)&displayMode, (int)EdgeErrorDisplay::Normalized))
                {
                    displayMode = EdgeErrorDisplay::Normalized;
                }
                if (ImGui::RadioButton("Scaled", (int *)&displayMode, (int)EdgeErrorDisplay::Scaled))
                {
                    displayMode = EdgeErrorDisplay::Scaled;
                }
                if (ImGui::RadioButton("Binary", (int *)&displayMode, (int)EdgeErrorDisplay::Binary))
                {
                    displayMode = EdgeErrorDisplay::Binary;
                }
            }
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("eps", &appState.mergeSettings.singleMergeErrorThreshold, 0.00001f, 0.00001f, 0.1f, "%.5f");
            ImGui::Checkbox("Show motorcycle edges", &appState.mergeSettings.showMotorcycleEdges);

            ImGui::EndGroup();
            if (!showErrorMap)
            {
                appState.mergeProcess = MergeProcess::Merging;
            }
        }
        ImGui::End();
    }
}

void showCurvesTab(bool isRenderCurves, bool firstRender, GmsAppState &appState)
{
    if (ImGui::BeginTabItem(renderModeStrings[RENDER_CURVES], nullptr, firstRender && isRenderCurves ? ImGuiTabItemFlags_SetSelected : 0))
    {
        appState.currentMode = {RENDER_CURVES};

        ImGui::SliderFloat("Curve width", &appState.curveLineWidth, 0.0f, 20.0f);
        ImGui::Checkbox("Show AABB", &appState.showCurveAABB);

        if (ImGui::RadioButton("Bézier", appState.curveRenderParams.curveMode == CurveRenderer::Bezier))
        {
            appState.curveRenderParams.curveMode = CurveRenderer::Bezier;
            appState.updateCurveRender();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Hermite", appState.curveRenderParams.curveMode == CurveRenderer::Hermite))
        {
            appState.curveRenderParams.curveMode = CurveRenderer::Hermite;
            appState.updateCurveRender();
        }

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

        auto curves = appState.curveRenderParams.curves;
        ImGui::Text("Control Points for Curve 0:");

        for (int i = 0; i < 4; ++i)
        {
            const auto &point = curves[0].getP(i);
            std::string label = "P" + std::to_string(i) + ": ";
            std::array<char, 64> textboxValue; // Adjust size as needed
            snprintf(textboxValue.data(), textboxValue.size(), "(%.2f, %.2f)", point.coords.x, point.coords.y);
            ImGui::PushID(i);
            ImGui::Text(label.c_str());
            ImGui::SameLine();
            ImGui::InputText(("##Point" + std::to_string(i)).c_str(), textboxValue.data(), textboxValue.size());
            ImGui::PopID();
        }

        ImGui::EndTabItem();
    }
}

void showPatchesTab(bool isRenderCurves, bool firstRender, GmsAppState &appState)
{
    if (ImGui::BeginTabItem(renderModeStrings[RENDER_PATCHES], nullptr, firstRender && !isRenderCurves ? ImGuiTabItemFlags_SetSelected : 0))
    {
        appState.currentMode = {RENDER_PATCHES};
        showRenderSettings(appState);
        showTestingMenu(appState);
        showMergingMenu(appState);
        showDebuggingMenu(appState);
        showComponentSelect(appState);
        showDCELInfo(appState);
        ImGui::EndTabItem();
    }
}

void GmsGui::showRightBar()
{
    ImGui::SetNextWindowPos(ImVec2(GUI_POS, 0));
    ImGui::SetNextWindowSize(ImVec2(GUI_WIDTH, SCR_HEIGHT));
    ImGui::Begin("gms", nullptr, ImGuiWindowFlags_NoTitleBar);
    ImGui::Spacing();

    static bool firstRender = true;
    if (ImGui::BeginTabBar("Rendering Mode"))
    {
        bool isRenderCurves = appState.currentMode == RENDER_CURVES;
        showCurvesTab(isRenderCurves, firstRender, appState);
        showPatchesTab(isRenderCurves, firstRender, appState);
        firstRender = false;
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void showRenderSettings(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Render Settings"))
    {
        ImGui::Spacing();
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
            ImGui::SliderFloat("Curve width", &appState.curveLineWidth, 0.0f, 20.0f);
        }
        ImGui::Checkbox("Draw patches", &appState.renderPatches);
        ImGui::Spacing();
    }
}
void showHermiteMatrixTable(GmsAppState &appState)
{
    int patchId = appState.userSelectedId.patchId;
    if (patchId == -1)
    {
        ImGui::Text("No patch selected");
        return;
    }
    ImGui::Spacing();
    if (ImGui::TreeNode("Show Hermite control matrix"))
    {
        static int selected_index = -1; // Stores the index of the currently selected item

        ImGui::Spacing();
        auto &selectedPatch = appState.patches[patchId].getControlMatrix();
        if (ImGui::BeginTable("HermiteTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
        {
            for (int i = 0; i < 16; i++)
            {
                char label[32];
                sprintf(label, "%.2f,%.2f", selectedPatch[i].coords.x, selectedPatch[i].coords.y);
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

            glm::vec3 patchColor = selectedPatch[selected_index].color;
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
        ImGui::TreePop();
    }
    ImGui::Spacing();
}

void showComponentSelect(GmsAppState &appState)
{
    ImGui::BeginDisabled(appState.manualEdgeSelect);
    if (ImGui::CollapsingHeader("Component Select", appState.componentSelectOptions.on ? ImGuiTreeNodeFlags_DefaultOpen : 0))
    {
        appState.componentSelectOptions.on = true;

        ImGui::Spacing();
        ImGui::Text("Select Component:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Patch", appState.componentSelectOptions.type == ComponentSelectOptions::Type::Patch))
        {
            appState.resetUserSelectedCurve();
            appState.componentSelectOptions.type = ComponentSelectOptions::Type::Patch;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Curve", appState.componentSelectOptions.type == ComponentSelectOptions::Type::Curve))
        {
            appState.componentSelectOptions.type = ComponentSelectOptions::Type::Curve;
        }
        ImGui::Spacing();
        showHermiteMatrixTable(appState);
        ImGui::Separator();

        if (appState.isCompSelect(ComponentSelectOptions::Type::Patch))
        {
            ImGui::Text("Patch Options:");
            ImGui::Checkbox("Show bounding box", &appState.componentSelectOptions.showPatchAABB);
            ImGui::Checkbox("Show tensor product region", &appState.componentSelectOptions.showMaxProductRegion);
            auto selectedRegion = appState.findSelectedRegion();
            if (selectedRegion)
            {
                auto regionAttributes = (*selectedRegion).allRegionAttributes;
                if (!regionAttributes.empty())
                {
                    auto &counter = appState.selectedTPRIdx;
                    if (counter >= regionAttributes.size())
                        counter = 0;

                    ImGui ::Spacing();
                    if (ImGui::BeginTable("RegionTable", 3, ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableNextColumn();
                        ImGui::Text("Tensor product region %d/%d", counter + 1, regionAttributes.size());

                        ImGui::TableNextColumn();
                        if (counter > 0)
                        {
                            if (ImGui::ArrowButton("##left_arrow", ImGuiDir_Left))
                                counter--;
                        }
                        else
                        {
                            ImGui::InvisibleButton("##left_placeholder", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                        }

                        ImGui::TableNextColumn();
                        if (counter < regionAttributes.size() - 1)
                        {
                            if (ImGui::ArrowButton("##right_arrow", ImGuiDir_Right))
                                counter++;
                        }
                        else
                        {
                            ImGui::InvisibleButton("##right_placeholder", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                        }

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("Number of patches: %d", regionAttributes[counter].getMaxPatches());
                        ImGui::Text("Region error: %.4f", regionAttributes[counter].error);

                        ImGui::EndTable();
                    }
                }
                else
                {
                    ImGui::Text("Selected region cannot be merged.");
                }
            }
        }

        // Spacing for better separation
        ImGui::Spacing();
    }

    else if (appState.componentSelectOptions.on)
    {
        appState.resetUserSelectedCurve();
        appState.componentSelectOptions.on = false;
    }
    ImGui::EndDisabled();
}

void GmsGui::showWindowMenuBar()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(GL_LENGTH, 20));

    fileDialog.SetTitle("title");
    fileDialog.SetTypeFilters({".hemesh"});

    ImGui::Begin("Gradient mesh renderer", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open mesh", "o"))
            {
                fileDialogMode = 0;
                fileDialog.Open();
            }
            else if (ImGui::MenuItem("Save rasterized image", "s"))
            {
                saveImage((std::string{IMAGE_DIR} + "/" + appState.meshname + ".png").c_str(), GL_LENGTH, GL_LENGTH);
            }
            else if (ImGui::MenuItem("Load motorcycle graph data", "m"))
            {
                appState.mergeProcess = MergeProcess::LoadProductRegionsPreprocessing;
            }
            else if (ImGui::MenuItem("Load tensor product regions data", "l"))
            {
                appState.mergeProcess = MergeProcess::LoadProductRegionsPreprocessing;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Preprocess single edge error", "", false, appState.preprocessSingleMergeProgress == -1))
            {
                appState.preprocessSingleMergeProgress = 0;
                appState.mergeProcess = MergeProcess::PreprocessSingleMerge;
            }
            if (ImGui::MenuItem("Preprocess tensor product regions", "", false, appState.preprocessProductRegionsProgress == -1))
            {
                appState.mergeProcess = MergeProcess::PreprocessProductRegions;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("View merge error map", "", false, appState.preprocessSingleMergeProgress == -2))
            {
                appState.mergeProcess = MergeProcess::ViewEdgeMap;
            }
            if (ImGui::MenuItem("View conflict graph stats", "", false, appState.preprocessProductRegionsProgress == -2.0f))
            {
                appState.mergeProcess = MergeProcess::ViewConflictGraphStats;
            }
            ImGui::EndMenu();
        }

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(("test" + appState.filename).c_str()).x);
        ImGui::Text("%s", appState.filename.c_str());
        ImGui::Button(std::to_string(appState.maxHWTessellation).c_str());
        ImGui::SetItemTooltip("Maximum hardware tessellation level");
        ImGui::EndMenuBar();
    }
    ImGui::End();

    fileDialog.Display();
    if (fileDialog.HasSelected())
    {
        std::cout << "Selected filename" << fileDialog.GetSelected().string() << std::endl;
        appState.filename = fileDialog.GetSelected().string();
        appState.filenameChanged = true;
        fileDialog.ClearSelected();
    }
}

void createImguiContext(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    ImFontConfig Config;
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    Config.MergeMode = true;
    mainFont = io.Fonts->AddFontFromFileTTF("../libs/imgui/misc/fonts/Cousine-Regular.ttf", 14);
    iconFont = io.Fonts->AddFontFromFileTTF("../assets/fonts/otfs/Font Awesome 6 Free-Solid-900.otf", 14.0f, &Config, icons_ranges);
    ImGui::GetIO().Fonts->Build();

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

void processKeyInput(GLFWwindow *window, GmsAppState &appState, ImGui::FileBrowser &fileDialog)
{
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
    {
        fileDialog.Open();
        fileDialogMode = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        saveImage((std::string{IMAGE_DIR} + "/" + appState.meshname + ".png").c_str(), GL_LENGTH, GL_LENGTH);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
    {
        appState.mergeProcess = MergeProcess::LoadProductRegionsPreprocessing;
    }

    static bool mKeyWasPressed = false;
    int mKeyState = glfwGetKey(window, GLFW_KEY_M);
    if (mKeyState == GLFW_PRESS && !mKeyWasPressed)
    {
        if (appState.manualEdgeSelect && !appState.candidateMerges.empty())
        {
            appState.mergeMode = MANUAL;
        }
        mKeyWasPressed = true; // Update the flag to indicate the key is held down
    }
    else if (mKeyState == GLFW_RELEASE && mKeyWasPressed)
    {
        mKeyWasPressed = false; // Reset the flag to indicate the key is no longer pressed
    }
}

void showDifferentMergingMethods(GmsAppState &appState)
{
    if (appState.preprocessProductRegionsProgress > -1)
    {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Finding tensor product regions...");
        if (appState.edgeRegions.empty())
            ImGui::ProgressBar(0, ImVec2(-1.0f, 0.0f));
        else
            ImGui::ProgressBar(appState.preprocessProductRegionsProgress, ImVec2(-1.0f, 0.0f));
        ImGui::Spacing();
        return;
    }
    switch (edge_select_current)
    {
    case MANUAL:
    {

        if (appState.selectedEdgeId == -1 && appState.candidateMerges.size() > 0)
        {
            appState.selectedEdgeId = 0;
        }

        /*
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::InputInt(" ", &appState.selectedEdgeId))
                {
                    appState.selectedEdgeId = std::max(0, std::min<int>(appState.selectedEdgeId, static_cast<int>(appState.candidateMerges.size()) - 1));
                }

                ImGui::SameLine();
                */
        if (appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Merge edge"))
            {
                appState.mergeMode = MANUAL;
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Click on an edge to select it. Press M to perform merge with selected edge.");
            ImGui::EndTooltip();
        }

        if (ImGui::IsMouseClicked(1))
        {
            showManualMerge = true;
            manualMergeWindow = ImGui::GetMousePos();
        }
        else if (ImGui::IsMouseClicked(0))
            showManualMerge = false;

        if (showManualMerge && appState.userSelectedId.curveId != -1)
        {
            appState.mergeProcess = MergeProcess::PreviewMerge;
            ImVec2 windowSize = ImGui::GetIO().DisplaySize;
            ImVec2 menuPos;
            if (manualMergeWindow.y > windowSize.y / 2)
            {
                menuPos.y = manualMergeWindow.y - 350.0f;
            }
            else
            {
                menuPos.y = manualMergeWindow.y + 10.0f;
            }

            if (manualMergeWindow.x > windowSize.x / 2)
            {
                menuPos.x = manualMergeWindow.x - 630.0f;
            }
            else
            {
                menuPos.x = manualMergeWindow.x + 10.0f;
            }

            ImGui::SetNextWindowPos(menuPos, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.9f);
            ImGui::SetNextWindowSize(ImVec2(250, 165), ImGuiCond_Once);
            bool showConflictGraphStats = true;
            ImGui::Begin("Context Menu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
            if (ImGui::BeginTable("ImageTable2", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Before", ImGuiTableColumnFlags_WidthFixed, GUI_PREVIEW_IMAGE_SIZE);
                std::string afterCol = "After (Error = " + std::to_string(appState.mergePreviewError) + ")";
                ImGui::TableSetupColumn(afterCol.c_str(), ImGuiTableColumnFlags_WidthFixed, GUI_PREVIEW_IMAGE_SIZE);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow(ImGuiTableRowFlags_None, GUI_PREVIEW_IMAGE_SIZE + 10);
                ImGui::TableSetColumnIndex(0);
                GLuint texture = LoadTextureFromFile(ORIG_IMG);
                ImGui::Image((ImTextureID)texture, ImVec2(GUI_PREVIEW_IMAGE_SIZE, GUI_PREVIEW_IMAGE_SIZE));
                ImGui::TableSetColumnIndex(1);
                GLuint texture2 = LoadTextureFromFile(CURR_IMG);
                ImGui::Image((ImTextureID)texture2, ImVec2(GUI_PREVIEW_IMAGE_SIZE, GUI_PREVIEW_IMAGE_SIZE));

                ImGui::EndTable();
            }

            ImGui::End();
        }
    }
    break;
    case RANDOM:
    {
        if (appState.mergeMode == RANDOM && appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Stop selection"))
                appState.mergeMode = NONE;
        }
        else if (appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Start selection"))
            {
                appState.mergeStatus = NA;
                appState.mergeMode = RANDOM;
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
    }
    break;
    case GRID:
        if (appState.mergeMode == GRID && appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Stop selection"))
                appState.mergeMode = NONE;
        }
        else if (appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Start selection"))
            {
                appState.mergeMode = GRID;
                appState.startTime = std::chrono::high_resolution_clock::now();
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        break;
    case DUAL_GRID:
        if (appState.mergeMode == DUAL_GRID && appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Stop selection"))
                appState.mergeMode = NONE;
        }
        else if (appState.candidateMerges.size() > 0)
        {
            if (ImGui::Button("Start selection"))
            {
                appState.mergeMode = DUAL_GRID;
                appState.startTime = std::chrono::high_resolution_clock::now();
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        break;
    case 4:
        if (true)
        {
            ImGui::BeginDisabled(appState.preprocessSingleMergeProgress != -2.0f);
            if (ImGui::Button("Start merge"))
            {
                appState.mergeProcess = MergeProcess::MergeMotorcycle;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Before merging process, single edge errors must be generated through the Edit menu.");
                ImGui::EndTooltip();
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        break;
    case 5:
        if (!appState.maxProductRegionsDone())
        {
            ImGui::BeginDisabled(appState.preprocessProductRegionsProgress != -2.0f);
            if (ImGui::Button("Start greedy merge"))
            {
                appState.mergeProcess = MergeProcess::MergeGreedyQuadError;
            }

            // if (ImGui::Button("Debug"))
            // appState.mergeProcess = MergeProcess::DebugMergeGreedyQuadError;
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Before merging process, tensor product regions must be generated through the Edit menu.");
                ImGui::EndTooltip();
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        break;
    case 6:
        if (!appState.maxProductRegionsDone())
        {
            ImGui::BeginDisabled(appState.preprocessProductRegionsProgress != -2.0f);
            if (ImGui::Button("Start region merge"))
            {
                appState.mergeProcess = MergeProcess::MergeTPRs;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Before merging process, tensor product regions must be generated through the Edit menu.");
                ImGui::EndTooltip();
            }

            if (appState.oneStepQuadErrorProgress > -1)
            {
                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text("Finding best merges...");
                if (appState.edgeRegions.empty())
                    ImGui::ProgressBar(0, ImVec2(-1.0f, 0.0f));
                else
                    ImGui::ProgressBar(appState.oneStepQuadErrorProgress, ImVec2(-1.0f, 0.0f));
                ImGui::Spacing();
                return;
            }
        }
        else
        {
            ImGui::Text("No merges possible");
        }
        break;
    }
}

void showMergingMenu(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Merging", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (appState.preprocessSingleMergeProgress > -1)
        {
            ImGui::Spacing();
            ImGui::Text("Preprocessing edges...");
            ImGui::ProgressBar(static_cast<float>(appState.preprocessSingleMergeProgress) / appState.candidateMerges.size(), ImVec2(-1.0f, 0.0f));
            ImGui::Spacing();
            return;
        }
        ImGui::Spacing();

        ImGui::Text("Edge select mode");
        ImGui::SetNextItemWidth(155.0f);
        // ImGui::Combo("##edgeselect", &edge_select_current, edge_select_items, IM_ARRAYSIZE(edge_select_items));

        if (ImGui::BeginCombo("##edgeselect", edge_select_items[edge_select_current]))
        { // Open the dropdown
            for (int i = 0; i < IM_ARRAYSIZE(edge_select_items); ++i)
            {
                if (i == 4 || i == 1 || i == 5)
                { // Insert separator before the 5th item
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                // Handle selection
                if (ImGui::Selectable(edge_select_items[i], edge_select_current == i))
                {
                    edge_select_current = i; // Update the current selection
                }
            }
            ImGui::EndCombo(); // Close the dropdown
        }
        ImGui::SameLine();
        appState.manualEdgeSelect = edge_select_current == MANUAL;
        if (appState.manualEdgeSelect)
        {
            appState.componentSelectOptions.on = false;
        }

        showDifferentMergingMethods(appState);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        AABB aabb = appState.mergeSettings.aabb;
        ImGui::Checkbox("Use pixel-based error metrics", &appState.useError);
        if (appState.useError)
        {
            ImGui::BeginDisabled(appState.mergeMode != NONE || appState.preprocessProductRegionsProgress > -1);
            ImGui::Spacing();
            ImGui::PushItemWidth(INPUT_WIDTH);
            static int item_current = static_cast<int>(appState.mergeSettings.metricMode);
            if (ImGui::Combo("Metric mode", &item_current, metric_mode_items, IM_ARRAYSIZE(metric_mode_items)))
            {
                appState.mergeSettings.metricMode = static_cast<MergeMetrics::MetricMode>(item_current);
            }
            static int item_image_region = static_cast<int>(appState.mergeSettings.pixelRegion);
            const char *image_region_items[] = {"Global", "Local"};
            /*
            if (ImGui::Combo("Pixel region AABB", &item_image_region, image_region_items, IM_ARRAYSIZE(image_region_items)))
            {
                appState.mergeSettings.pixelRegion = static_cast<MergeMetrics::PixelRegion>(item_image_region);
            }
            */

            if (edge_select_current == 5 || edge_select_current == 6)
            {
                ImGui::PushItemWidth(INPUT_WIDTH / 2);
                ImGui::DragFloat("Quads", &appState.quadErrorWeight, 0.01f, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                float complementWeight = 1.0f - appState.quadErrorWeight; // Calculate complement
                if (ImGui::DragFloat("Error Weight", &complementWeight, 0.01f, 0.0f, 1.0f, "%.2f"))
                {
                    appState.quadErrorWeight = 1.0f - complementWeight;
                }
                ImGui::PopItemWidth();
            }
            ImGui::DragFloat("Error threshold", &appState.mergeSettings.errorThreshold, 0.0001f, 0.0001f, 0.1f, "%.4f");
            ImGui::DragInt("Pooling resolution", &appState.mergeSettings.poolRes, 1.0f, 100, 1000);
            // ImGui::DragFloat("AABB padding", &appState.mergeSettings.aabbPadding, 0.01f, 0.0f, 0.1f);
            ImGui::PopItemWidth();

            ImGui::EndDisabled();

            ImGui::Spacing();
            /*
            if (ImGui::TreeNode("Show pixel metric images"))
            {
                ImGui::Spacing();
                ImGui::Text("AABB: [(%.3f, %.3f), (%.3f, %.3f)]", aabb.min.x, aabb.min.y, aabb.max.x, aabb.max.y);
                if (ImGui::BeginTable("mergedImageTable", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Unmerged Image", ImGuiTableColumnFlags_WidthFixed, GUI_IMAGE_SIZE);
                    ImGui::TableSetupColumn("Merged Image", ImGuiTableColumnFlags_WidthFixed, GUI_IMAGE_SIZE);
                    if (appState.mergeStatus != MergeStatus::CYCLE)
                    {
                        ImGui::TableHeadersRow();
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, GUI_IMAGE_SIZE + 10);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Image((void *)(intptr_t)appState.patchRenderResources.unmergedTexture, ImVec2(GUI_IMAGE_SIZE, GUI_IMAGE_SIZE)); // Fixed size
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Image((void *)(intptr_t)appState.patchRenderResources.mergedTexture, ImVec2(GUI_IMAGE_SIZE, GUI_IMAGE_SIZE)); // Fixed size
                    }
                    else
                    {
                        ImGui::TableHeadersRow();
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, GUI_IMAGE_SIZE + 10);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Cycle - N/A");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("Cycle - N/A");
                    }
                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }
            */
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (appState.numOfMerges > 0)
        {
            ImGui::Text("Merge iteration: %d", appState.numOfMerges);

            if (ImGui::BeginTable("MergeTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("swagt", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("swag2", ImGuiTableColumnFlags_WidthFixed, 170.0f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Merge status");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(toString(appState.mergeStatus));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Merge error");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.5f", appState.mergeError);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Attempted merges");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", appState.attemptedMergesIdx);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Candidate edge merges");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", appState.candidateMerges.size());

                ImGui::EndTable();
            }
            ImGui::Spacing();
            if (ImGui::Button("Compare images"))
            {
                showFinalImages = true; // Set the flag to open the new window
            }
        }
        else if (appState.regionsMerged > 0)
        {
            if (ImGui::BeginTable("MergeTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("swagt", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("swag2", ImGuiTableColumnFlags_WidthFixed, 170.0f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Regions merged");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", appState.regionsMerged);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Merge error");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.5f", appState.mergeError);

                ImGui::EndTable();
            }
            ImGui::Spacing();
            if (ImGui::Button("Compare images"))
            {
                showFinalImages = true; // Set the flag to open the new window
            }
        }
        else
        {
            ImGui::Text("No merges applied yet");
            if (ImGui::BeginTable("MergeTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("swagt", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("swag2", ImGuiTableColumnFlags_WidthFixed, 170.0f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
                ImGui::Text("Candidate edge merges");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", appState.candidateMerges.size());

                ImGui::EndTable();
            }
        }

        ImGui::Spacing();
        static bool showCandidateMerges = false;
        static bool prevShowCandidateMerges = showCandidateMerges;
        ImGui::Checkbox("Show candidate merges", &showCandidateMerges);
        if (prevShowCandidateMerges != showCandidateMerges)
        {
            if (showCandidateMerges)
            {
                appState.showAllCandidateEdges(blue);
            }
            else
            {
                appState.showAllCandidateEdges(black);
            }
            prevShowCandidateMerges = showCandidateMerges;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Reset mesh"))
        {
            appState.filenameChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT) && appState.currentSave != 0)
        {
            appState.loadSave = true;
            appState.filename = "mesh_saves/save_" + std::to_string(--appState.currentSave) + ".hemesh";
        }
        ImGui::SameLine();
        ImGui::Button(ICON_FA_ARROW_ROTATE_RIGHT);

        ImGui::Spacing();
    }
}

void showDebuggingMenu(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Debugging"))
    {
        ImGui::Spacing();
        ImGui::Text("Previous merge: ");
        ImGui::Spacing();
        showPreviousMergeInfo(appState.mergeStats);
    }
}

void showTestingMenu(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Testing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::BeginDisabled(appState.mergeProcess == MergeProcess::RandomTest || appState.mergeProcess == MergeProcess::GridTest || appState.mergeProcess == MergeProcess::DualGridTest);
        if (ImGui::Button("Random test"))
        {
            appState.mergeMode = NONE;
            appState.mergeProcess = MergeProcess::RandomTest;
        }
        if (ImGui::Button("Grid test"))
        {
            appState.mergeMode = NONE;
            appState.mergeProcess = MergeProcess::GridTest;
        }
        if (ImGui::Button("Dual grid test"))
        {
            appState.mergeMode = NONE;
            appState.mergeProcess = MergeProcess::DualGridTest;
        }
        ImGui::EndDisabled();
        ImGui::Spacing();
    }
}

void showDCELInfo(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Mesh Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        showGradMeshInfo(appState);
    }
}

void showPreviousMergeInfo(GmsAppState::MergeStats &stats)
{
    if (ImGui::BeginTable("MergeStats", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("Half-edge id");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", stats.mergedHalfEdgeIdx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("t");
        ImGui::TableSetColumnIndex(1);
        if (stats.t == -1)
            ImGui::Text("None");
        else
            ImGui::Text("%f", stats.t);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("Removed face id");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", stats.removedFaceId);
        ImGui::EndTable();
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("TableWithWidths", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        // Define column widths
        ImGui::TableSetupColumn("Edge", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("t-junc t", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Case", ImGuiTableColumnFlags_WidthFixed, 200.0f);

        ImGui::TableNextRow(); // Setup the next row
        ImGui::TableSetColumnIndex(0);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Dark blue color
        ImGui::Text("Edge");

        ImGui::TableSetColumnIndex(1);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Dark blue color
        ImGui::Text("T-junc t");

        ImGui::TableSetColumnIndex(2);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Dark blue color
        ImGui::Text("Case");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Top edge");
        ImGui::TableSetColumnIndex(1);
        if (stats.topEdgeT)
            ImGui::Text("%.3f", stats.topEdgeT);
        else
            ImGui::Text("None");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", toString(stats.topEdgeCase));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Bottom edge");
        ImGui::TableSetColumnIndex(1);
        if (stats.bottomEdgeT)
            ImGui::Text("%.3f", stats.bottomEdgeT);
        else
            ImGui::Text("None");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", toString(stats.bottomEdgeCase));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
}

void showGradMeshInfo(GmsAppState &appState)
{
    const GradMesh &mesh = appState.mesh;
    if (ImGui::BeginTabBar("DCEL Components"))
    {
        static bool setDefaultTab = true;
        if (ImGui::BeginTabItem("Points"))
        {
            std::vector<int> pointIdxs = getValidCompIndices(mesh.getPoints());
            std::vector<std::string> dynamicItems;
            std::vector<const char *> items;
            createListItems(pointIdxs, dynamicItems, items, "p");

            static int item_selected_idx = 0; // Here we store our selected data as an index.
            int item_highlighted_idx = -1;    // Here we store our highlighted data as an index.
            ImGui::Spacing();
            createListBox(items, item_selected_idx, item_highlighted_idx);
            ImGui::SameLine();
            setComponentText(mesh.getPoints(), item_selected_idx, pointIdxs);

            ImGui::Spacing();
            ImGui::Text("%d points", pointIdxs.size());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Handles"))
        {
            std::vector<int> handleIdxs = getValidCompIndices(mesh.getHandles());
            std::vector<std::string> dynamicItems;
            std::vector<const char *> items;
            createListItems(handleIdxs, dynamicItems, items, "h");

            static int item_selected_idx = 0; // Here we store our selected data as an index.
            int item_highlighted_idx = -1;    // Here we store our highlighted data as an index.
            ImGui::Spacing();
            createListBox(items, item_selected_idx, item_highlighted_idx);
            ImGui::SameLine();
            setComponentText(mesh.getHandles(), item_selected_idx, handleIdxs);

            ImGui::Spacing();
            ImGui::Text("%d handles", handleIdxs.size());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Faces"))
        {
            std::vector<int> faceIdxs = getValidCompIndices(mesh.getFaces());
            std::vector<std::string> dynamicItems;
            std::vector<const char *> items;
            createListItems(faceIdxs, dynamicItems, items, "f");

            int item_highlighted_idx = -1; // Here we store our highlighted data as an index.
            int selectedFaceIdx = -1;
            if (appState.userSelectedId.patchId != -1)
            {
                selectedFaceIdx = appState.patches[appState.userSelectedId.patchId].getFaceIdx();
            }
            ImGui::Spacing();
            ImGui::Columns(2, nullptr, true);
            ImGui::SetColumnWidth(0, 100);
            bool selectedFaceChanged = createListBox(items, selectedFaceIdx, item_highlighted_idx);
            if (selectedFaceChanged && selectedFaceIdx != appState.patches[appState.userSelectedId.patchId].getFaceIdx())
            {
                appState.setPatchId(getPatchFromFaceIdx(appState.patches, selectedFaceIdx));
            }
            ImGui::Spacing();
            ImGui::Text("%d faces", faceIdxs.size());

            ImGui::NextColumn();

            if (selectedFaceIdx != -1)
                setFaceInfo(mesh.getFaces(), selectedFaceIdx, faceIdxs);
            ImGui::Columns(1);

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Half-edges", nullptr, setDefaultTab ? ImGuiTabItemFlags_SetSelected : 0))
        {
            const std::vector<HalfEdge> &halfedges = mesh.getEdges();
            std::vector<int> edgeIdxs = getValidCompIndices(halfedges);
            std::vector<std::string> dynamicItems;
            std::vector<const char *> items;
            createListItems(edgeIdxs, dynamicItems, items, "e");

            int item_highlighted_idx = -1; // Here we store our highlighted data as an index.
            auto userId = appState.userSelectedId;
            if (!userId.isNull())
            {
                auto &selectedPatch = appState.patches[userId.patchId];
                for (int i = 0; i < edgeIdxs.size(); i++)
                {
                    if (edgeIdxs[i] == selectedPatch.getCurve(userId.curveId).getHalfEdgeIdx())
                    {
                        selectedHalfEdgeIdx = i;
                    }
                }
            }
            ImGui::Spacing();
            ImGui::Columns(2, nullptr, true);
            ImGui::SetColumnWidth(0, 100);
            bool selectedEdgeChanged = createListBox(items, selectedHalfEdgeIdx, item_highlighted_idx);
            if (selectedEdgeChanged && appState.isCompSelect(ComponentSelectOptions::Type::Curve))
            {
                appState.setUserCurveColor(black);
                int prevPatch = userId.patchId;
                appState.userSelectedId = getCurveIdFromEdgeIdx(appState.patches, edgeIdxs[selectedHalfEdgeIdx]);
                appState.setUserCurveColor(blue);
                appState.updateCurves({prevPatch, appState.userSelectedId.patchId});
            }
            else if (selectedEdgeChanged)
            {
                appState.userSelectedId = getCurveIdFromEdgeIdx(appState.patches, edgeIdxs[selectedHalfEdgeIdx]);
            }
            ImGui::Spacing();
            ImGui::Text("%d edges", edgeIdxs.size());
            ImGui::NextColumn();
            if (selectedHalfEdgeIdx != -1)
                setHalfEdgeInfo(halfedges, selectedHalfEdgeIdx, edgeIdxs);

            ImGui::Columns(1);

            ImGui::EndTabItem();
        }
        setDefaultTab = false;

        ImGui::EndTabBar();
    }
}

void pushColor(ButtonColor color)
{
    ImVec4 baseColor;

    switch (color)
    {
    case ButtonColor::OriginPoint:
        baseColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        break;
    case ButtonColor::Face:
        baseColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        break;
    case ButtonColor::Handles:
        baseColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        break;
    case ButtonColor::PrevNext:
        baseColor = ImVec4(0.7f, 0.8f, 1.0f, 1.0f); // Light Blue
        break;
    case ButtonColor::Twin:
        baseColor = ImVec4(0.4f, 0.7f, 0.9f, 1.0f); // Medium Blue
        break;
    case ButtonColor::Parent:
        baseColor = ImVec4(0.4f, 0.8f, 0.4f, 1.0f); // Medium Green
        break;
    case ButtonColor::Children:
        baseColor = ImVec4(0.7f, 0.9f, 0.7f, 1.0f); // Light Green
        break;
    default:
        baseColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Light Gray fallback
        break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(baseColor.x * 0.9f, baseColor.y * 0.9f, baseColor.z * 0.9f, baseColor.w));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(baseColor.x * 0.8f, baseColor.y * 0.8f, baseColor.z * 0.8f, baseColor.w));
}

void setFaceInfo(const auto &components, int &item_selected_idx, const auto &idxs)
{
    // if (item_selected_idx >= idxs.size())
    // item_selected_idx = idxs.size() - 1;
    auto &selectedEdge = components[idxs[item_selected_idx]];

    ImGui::Spacing();
    ImGui::Text(" f%d", idxs[item_selected_idx]);
    ImGui::Spacing();

    if (ImGui::BeginTable("Faces", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
    {
        ImGui::TableSetupColumn("FirstColumn", ImGuiTableColumnFlags_WidthFixed, 65.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        ImGui::Text("Half-edge");
        ImGui::TableSetColumnIndex(1);
        pushColor(ButtonColor::PrevNext);
        compButton(item_selected_idx, idxs, selectedEdge.halfEdgeIdx, "e");
        ImGui::PopStyleColor(3);

        ImGui::EndTable();
    }

    ImGui::Spacing();
}

void setHalfEdgeInfo(const auto &components, int &item_selected_idx, const auto &idxs)
{
    // if (item_selected_idx >= idxs.size())
    // item_selected_idx = idxs.size() - 1;
    auto &selectedEdge = components[idxs[item_selected_idx]];

    ImGui::Spacing();
    ImGui::Text(" e%d", idxs[item_selected_idx]);
    if (selectedEdge.isBar())
    {
        ImGui::SameLine();
        ImGui::Text("(Bar)");
    }
    else if (selectedEdge.isStemParent())
    {
        ImGui::SameLine();
        ImGui::Text("(Stem/Parent)");
    }
    else if (selectedEdge.isStem())
    {
        ImGui::SameLine();
        ImGui::Text("(Stem)");
    }
    else if (selectedEdge.isParent())
    {
        ImGui::SameLine();
        ImGui::Text("(Parent)");
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("HalfEdges", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
    {
        ImGui::TableSetupColumn("FirstColumn", ImGuiTableColumnFlags_WidthFixed, 65.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        ImGui::Text("Color");
        ImGui::TableSetColumnIndex(1);
        glm::vec3 color = selectedEdge.color;
        ImVec4 imColor = ImVec4(color.x, color.y, color.z, 1.00f);
        ImGui::ColorEdit3("", (float *)&imColor);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        ImGui::Text("Twist");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("(%.3f, %.3f)", selectedEdge.twist.coords.x, selectedEdge.twist.coords.y);
        glm::vec3 tcolor = selectedEdge.twist.color;
        ImVec4 imColor2 = ImVec4(tcolor.x, tcolor.y, tcolor.z, 1.00f);
        ImGui::ColorEdit3("", (float *)&imColor2);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        if (!selectedEdge.isChild())
        {
            // ImGui::Text(" ");
            // ImGui::SameLine();
            ImGui::Text("Origin");
            ImGui::TableSetColumnIndex(1);
            pushColor(ButtonColor::OriginPoint);
            compButton(item_selected_idx, idxs, selectedEdge.originIdx, "p");
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
        }
        else
        {
            ImGui::Text("Interval");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("[%.3f, %.3f]", selectedEdge.interval.x, selectedEdge.interval.y);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        ImGui::Text("Face");
        ImGui::TableSetColumnIndex(1);
        pushColor(ButtonColor::Face);
        compButton(item_selected_idx, idxs, selectedEdge.faceIdx, "f");
        ImGui::PopStyleColor(3);

        if (!selectedEdge.isBar())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
            ImGui::Text("Handles");
            ImGui::TableSetColumnIndex(1);
            pushColor(ButtonColor::Handles);
            compButton(item_selected_idx, idxs, selectedEdge.handleIdxs.first, "h");
            ImGui::SameLine();
            compButton(item_selected_idx, idxs, selectedEdge.handleIdxs.second, "h");
            ImGui::PopStyleColor(3);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
        ImGui::Text("Prev/Next");
        ImGui::TableSetColumnIndex(1);
        pushColor(ButtonColor::PrevNext);
        compButton(item_selected_idx, idxs, selectedEdge.prevIdx, ICON_FA_ARROW_LEFT " e");
        ImGui::SameLine();
        compButton(item_selected_idx, idxs, selectedEdge.nextIdx, ICON_FA_ARROW_RIGHT " e");
        ImGui::PopStyleColor(3);

        if (selectedEdge.hasTwin())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
            ImGui::Text("Twin");
            ImGui::TableSetColumnIndex(1);
            pushColor(ButtonColor::Twin);
            compButton(item_selected_idx, idxs, selectedEdge.twinIdx, ICON_FA_GRIP_LINES_VERTICAL " e");
            ImGui::PopStyleColor(3);
        }

        if (selectedEdge.isChild())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
            ImGui::Text("Parent");
            ImGui::TableSetColumnIndex(1);
            pushColor(ButtonColor::Parent);
            compButton(item_selected_idx, idxs, selectedEdge.parentIdx, "e");
            ImGui::PopStyleColor(3);
        }

        if (selectedEdge.isParent())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20));
            ImGui::Text("Children");
            ImGui::TableSetColumnIndex(1);
            pushColor(ButtonColor::Children);
            for (size_t i = 0; i < selectedEdge.childrenIdxs.size(); ++i)
            {
                if (i % 3 == 0 && i != 0)
                {
                    ImGui::Text("");
                }
                ImGui::SameLine();
                compButton(item_selected_idx, idxs, selectedEdge.childrenIdxs[i], "e");
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
}

void createListItems(std::vector<int> &idxs, std::vector<std::string> &dynamicItems, std::vector<const char *> &items, std::string ctype)
{
    for (int idx : idxs)
    {
        dynamicItems.push_back(ctype + std::to_string(idx));
    }
    for (const auto &str : dynamicItems)
    {
        items.push_back(str.c_str());
    }
}

bool createListBox(std::vector<const char *> &items, int &item_selected_idx, int &item_highlighted_idx)
{
    bool selectionChanged = false;
    if (ImGui::BeginListBox("##listbox 2", ImVec2(80.0f, 12 * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (int n = 0; n < items.size(); n++)
        {
            const bool is_selected = (item_selected_idx == n);
            if (ImGui::Selectable(items[n], is_selected))
            {
                item_selected_idx = n;
                selectionChanged = true;
            }

            if (ImGui::IsItemHovered())
                item_highlighted_idx = n;

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }
    return selectionChanged;
}

void setComponentText(const auto &components, int &item_selected_idx, const auto &idxs)
{
    if (item_selected_idx >= idxs.size())
        item_selected_idx = idxs.size() - 1;
    std::stringstream ss;
    ss << components[idxs[item_selected_idx]];
    std::string output = ss.str(); // Convert to string
    ImGui::Text("%s", output.c_str());
}

void compButton(int &item_selected_idx, const auto &idxs, const int findIdx, std::string text)
{
    std::string buttonText = text + std::to_string(findIdx);
    if (ImGui::Button(buttonText.c_str()))
    {
        auto it = std::find(idxs.begin(), idxs.end(), findIdx);
        item_selected_idx = std::distance(idxs.begin(), it);
    }
}
