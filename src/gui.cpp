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

    processKeyInput(gmsWindow.getGLFWwindow(), appState, fileDialog);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

void setCompButton(int &item_selected_idx, const auto &idxs, const int findIdx)
{
    ImGui::SameLine();
    if (ImGui::Button(std::to_string(findIdx).c_str()))
    {
        auto it = std::find(idxs.begin(), idxs.end(), findIdx);
        item_selected_idx = std::distance(idxs.begin(), it);
    }
}

void setHalfEdgeInfo(const auto &components, int &item_selected_idx, const auto &idxs)
{
    if (item_selected_idx >= idxs.size())
        item_selected_idx = idxs.size() - 1;
    auto &selectedEdge = components[idxs[item_selected_idx]];

    std::stringstream ss;
    ss << selectedEdge;
    std::string output = ss.str(); // Convert to string
    ImGui::Text("%s", output.c_str());

    glm::vec3 color = selectedEdge.color;
    ImVec4 imColor = ImVec4(color.x, color.y, color.z, 1.00f);
    ImGui::Text(" ");
    ImGui::SameLine();
    ImGui::ColorEdit3("Color", (float *)&imColor);
    ImGui::Spacing();

    ImGui::Text("  Prev Index:");
    setCompButton(item_selected_idx, idxs, selectedEdge.prevIdx);
    ImGui::Text("  Next Index:");
    setCompButton(item_selected_idx, idxs, selectedEdge.nextIdx);
    if (selectedEdge.hasTwin())
    {
        ImGui::Text("  Twin Index:");
        setCompButton(item_selected_idx, idxs, selectedEdge.twinIdx);
    }
    if (selectedEdge.isChild())
    {
        ImGui::Text("  Parent Index:");
        setCompButton(item_selected_idx, idxs, selectedEdge.parentIdx);
    }
    if (selectedEdge.isParent())
    {
        ImGui::Text("  Children Indices:");
        ImGui::Text(" ");
        ImGui::SameLine();
        for (size_t i = 0; i < selectedEdge.childrenIdxs.size(); ++i)
        {
            setCompButton(item_selected_idx, idxs, selectedEdge.childrenIdxs[i]);
            ImGui::SameLine();
            if (i % 5 == 0 && i != 0)
            {
                ImGui::Text(" ");
                ImGui::Text(" ");
                ImGui::SameLine();
            }
        }
    }
}

void list(const GradMesh &mesh)
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
            if (appState.selectedPatchId != -1)
            {
                showHermiteMatrixTable();
            }
            else
            {
                ImGui::Text("No patch selected");
            }

            showRenderSettings();

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::Spacing();

            // This is the first level of the collapsing header
            if (ImGui::CollapsingHeader("Merging", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Spacing();

                // This is a nested collapsing header inside the previous one
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
                        // Code that executes when the integer changes
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
                            appState.mergeMode = RANDOM;
                    }
                    else
                    {
                        ImGui::Text("No merges possible");
                    }

                    ImGui::PopItemFlag();
                    ImGui::TreePop();
                }

                ImGui::Spacing();
                ImGui::Text("Candidate edge merges: %d", appState.numOfCandidateMerges);
                ImGui::Spacing();
            }

            ImGui::EndTabItem();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Debugging", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Spacing();
                ImGui::Text("Previous merge: ");
                ImGui::Spacing();

                if (ImGui::BeginTable("split1", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                {
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

                if (ImGui::BeginTable("split1", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                {
                    ImGui::TableSetupColumn("Edge");
                    ImGui::TableSetupColumn("Case");
                    ImGui::TableSetupColumn("t-junction t");
                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Top edge");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", appState.topEdgeCase.c_str());
                    ImGui::TableSetColumnIndex(2);
                    if (appState.topEdgeT)
                        ImGui::Text("%f", appState.topEdgeT);
                    else
                        ImGui::Text("None");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Bottom edge");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", appState.bottomEdgeCase.c_str());
                    ImGui::TableSetColumnIndex(2);
                    if (appState.bottomEdgeT)
                        ImGui::Text("%f", appState.bottomEdgeT);
                    else
                        ImGui::Text("None");

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Spacing();
                list(*appState.mesh);
            }
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
