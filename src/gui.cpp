#include "gui.hpp"

ImFont *iconFont;
ImFont *mainFont;

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

    processKeyInput(gmsWindow.getGLFWwindow(), appState, fileDialog);

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
    ImGui::Text("Maximum hardware tessellation level: %i", appState.maxHWTessellation);
    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::BeginTabBar("Rendering Mode"))
    {
        if (ImGui::BeginTabItem(modeNames[RENDER_PATCHES]))
        {
            appState.currentMode = {RENDER_PATCHES};

            // showHermiteMatrixTable(appState.selectedPatchId);

            showRenderSettings();
            ImGui::Spacing();
            AABB aabb = appState.meshAABB;
            ImGui::Text("AABB: min{%.3f, %.3f}, max {%.3f, %.3f}", aabb.min.x, aabb.min.y, aabb.max.x, aabb.max.y);

            ImGui::Spacing();
            ImGui::Spacing();

            showMergingMenu(appState);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();
            showDebuggingMenu(appState);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(modeNames[RENDER_CURVES]))
        {
            appState.currentMode = {RENDER_CURVES};
            ImGui::SliderFloat("Curve width", &appState.curveLineWidth, 0.0f, 10.0f);

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
        ImGui::EndTabBar();
    }

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

void GmsGui::showHermiteMatrixTable(int patchId)
{
    if (patchId == -1)
    {
        ImGui::Text("No patch selected");
        return;
    }
    static int selected_index = -1; // Stores the index of the currently selected item

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Selected patch: %i", appState.selectedPatchId);
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

    fileDialog.SetTitle("title");
    fileDialog.SetTypeFilters({".hemesh"});

    ImGui::Begin("Gradient mesh renderer", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
                fileDialog.Open();
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

void processKeyInput(GLFWwindow *window, const GmsAppState &appState, ImGui::FileBrowser &fileDialog)
{
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        fileDialog.Open();
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        saveImage((std::string{IMAGE_DIR} + "/" + extractFileName(appState.filename) + ".png").c_str(), GL_LENGTH, GL_LENGTH);
}

void showMergingMenu(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Merging", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();

        if (ImGui::TreeNode("Manual edge select"))
        {
            if (appState.selectedEdgeId == -1 && appState.numOfCandidateMerges > 0)
            {
                appState.selectedEdgeId = 0;
            }
            ImGui::Spacing();
            ImGui::Text("Select an edge:");
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputInt(" ", &appState.selectedEdgeId))
            {
                appState.selectedEdgeId = std::max(0, std::min(appState.selectedEdgeId, appState.numOfCandidateMerges - 1));
            }

            ImGui::SameLine();
            if (appState.numOfCandidateMerges > 0)
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

            ImGui::PopItemFlag();
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::TreeNode("Random edge select"))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            ImGui::Spacing();
            if (appState.mergeMode == RANDOM)
            {
                if (ImGui::Button("Stop random selection"))
                    appState.mergeMode = NONE;
            }
            else if (appState.numOfCandidateMerges > 0)
            {
                if (ImGui::Button("Start random selection"))
                {
                    appState.mergeMode = RANDOM;
                    appState.saveMerges = true;
                }
            }
            else
            {
                ImGui::Text("No merges possible");
            }

            ImGui::PopItemFlag();
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::TreeNode("Read list edge select"))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            ImGui::Spacing();
            ImGui::PushTextWrapPos();
            ImGui::Text("Reads a list of selected edges and performs merging on them. Mainly for debugging.");
            if (appState.mergeMode == EDGELIST)
            {
                if (ImGui::Button("Stop reading file"))
                    appState.mergeMode = NONE;
            }
            else
            {
                if (ImGui::Button("Read file for edge ids"))
                    appState.mergeMode = EDGELIST;
            }
            ImGui::PopTextWrapPos();

            ImGui::PopItemFlag();
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::BeginTable("split1", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
            ImGui::Text("Number of merges");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", appState.merges.size());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
            ImGui::Text("Candidate edge merges");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", appState.numOfCandidateMerges);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        if (ImGui::Button("Reset mesh"))
        {
            appState.filenameChanged = true;
        }
        ImGui::SameLine();
        ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT);
        ImGui::SameLine();
        ImGui::Button(ICON_FA_ARROW_ROTATE_RIGHT);
    }
}

void showDebuggingMenu(GmsAppState &appState)
{
    if (ImGui::CollapsingHeader("Debugging", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::Text("Previous merge: ");
        ImGui::Spacing();

        showPreviousMergeInfo(appState);
        showGradMeshInfo(*appState.mesh);
    }
}

void showPreviousMergeInfo(GmsAppState &appState)
{
    if (ImGui::BeginTable("split1", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("Half-edge id");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", appState.mergedHalfEdgeIdx);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("t");
        ImGui::TableSetColumnIndex(1);
        if (appState.t == -1)
            ImGui::Text("None");
        else
            ImGui::Text("%f", appState.t);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 100, 20)); // Red color
        ImGui::Text("Removed face id");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", appState.removedFaceId);
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
        if (appState.topEdgeT)
            ImGui::Text("%.3f", appState.topEdgeT);
        else
            ImGui::Text("None");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", appState.topEdgeCase.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Bottom edge");
        ImGui::TableSetColumnIndex(1);
        if (appState.bottomEdgeT)
            ImGui::Text("%.3f", appState.bottomEdgeT);
        else
            ImGui::Text("None");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", appState.bottomEdgeCase.c_str());
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
}

void showGradMeshInfo(const GradMesh &mesh)
{
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

            static int item_selected_idx = 0; // Here we store our selected data as an index.
            int item_highlighted_idx = -1;    // Here we store our highlighted data as an index.
            ImGui::Spacing();
            createListBox(items, item_selected_idx, item_highlighted_idx);
            ImGui::SameLine();
            setComponentText(mesh.getFaces(), item_selected_idx, faceIdxs);

            ImGui::Spacing();
            ImGui::Text("%d faces", faceIdxs.size());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Half-edges", nullptr, setDefaultTab ? ImGuiTabItemFlags_SetSelected : 0))
        {
            const std::vector<HalfEdge> &halfedges = mesh.getEdges();
            std::vector<int> edgeIdxs = getValidCompIndices(halfedges);
            std::vector<std::string> dynamicItems;
            std::vector<const char *> items;
            createListItems(edgeIdxs, dynamicItems, items, "e");

            static int item_selected_idx = 0; // Here we store our selected data as an index.
            int item_highlighted_idx = -1;    // Here we store our highlighted data as an index.
            ImGui::Spacing();
            ImGui::Columns(2, nullptr, true);
            ImGui::SetColumnWidth(0, 100);
            createListBox(items, item_selected_idx, item_highlighted_idx);
            ImGui::NextColumn();
            setHalfEdgeInfo(halfedges, item_selected_idx, edgeIdxs);
            ImGui::Columns(1);

            ImGui::Spacing();
            ImGui::Text("%d edges", edgeIdxs.size());
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

void setHalfEdgeInfo(const auto &components, int &item_selected_idx, const auto &idxs)
{
    if (item_selected_idx >= idxs.size())
        item_selected_idx = idxs.size() - 1;
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

    if (ImGui::BeginTable("split1", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
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

void createListBox(std::vector<const char *> &items, int &item_selected_idx, int &item_highlighted_idx)
{
    if (ImGui::BeginListBox("##listbox 2", ImVec2(80.0f, 12 * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (int n = 0; n < items.size(); n++)
        {
            const bool is_selected = (item_selected_idx == n);
            if (ImGui::Selectable(items[n], is_selected))
                item_selected_idx = n;

            if (ImGui::IsItemHovered())
                item_highlighted_idx = n;

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }
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
