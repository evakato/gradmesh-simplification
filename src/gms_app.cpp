#include "gms_app.hpp"

int numTests = 6;

GmsApp::GmsApp()
{
    setupDirectories();
    setupNewMesh();
}

void GmsApp::loadMesh()
{
    appState.resetMerges();
    appState.mesh = readHemeshFile(appState.filename);
    appState.updateMeshRender();
    appState.setUnmergedGlPatches();
    merger.startupMesh();
    appState.preprocessProductRegionsProgress = -1;
    preprocessor.setEdgeRegions();
}

void GmsApp::setupNewMesh()
{
    loadMesh();
    createDir(SAVES_DIR);
    writeHemeshFile(DEFAULT_FIRST_SAVE_DIR, appState.mesh);
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.loadSave)
            loadMesh();

        if (appState.filenameChanged)
            setupNewMesh();

        switch (appState.mergeProcess)
        {
        case MergeProcess::PreprocessSingleMerge:
            preprocessor.preprocessSingleMergeError();
            break;
        case MergeProcess::PreprocessProductRegions:
            preprocessor.preprocessProductRegions();
            break;
        case MergeProcess::LoadProductRegionsPreprocessing:
            preprocessor.loadProductRegionsPreprocessing();
            break;
        case MergeProcess::MergeTPRs:
            preprocessor.mergeGreedyQuadErrorOneStep();
            // merger.startupMesh();
            break;
        case MergeProcess::MergeGreedyQuadError:
            preprocessor.mergeGreedyQuadError();
            // merger.startupMesh();
            break;
        case MergeProcess::MergeMotorcycle:
            preprocessor.mergeMotorcycle();
            // merger.startupMesh();
            break;
        case MergeProcess::DebugMergeGreedyQuadError:
            preprocessor.mergeIndependentSet();
            // merger.startupMesh();
            break;
        case MergeProcess::ViewEdgeMap:
            merger.metrics.generateEdgeErrorMap(appState.edgeErrorDisplay);
            break;
        case MergeProcess::PreviewMerge:
            merger.previewMerge();
            break;
        case MergeProcess::RandomTest:
            runTest(RANDOM);
            break;
        case MergeProcess::GridTest:
            runTest(GRID);
            break;
        case MergeProcess::DualGridTest:
            runTest(DUAL_GRID);
            break;
        case MergeProcess::Merging:
            merger.merge();
            break;
        }

        switch (appState.currentMode)
        {
        case RENDER_CURVES:
            curveRenderer.render();
            break;
        case RENDER_PATCHES:
            patchRenderer.render(appState.patchRenderParams);
            break;
        }

        gui.render();
        gmsWindow.swapBuffers();
    }
}

void GmsApp::runTest(MergeSelectMode mode)
{
    static int randomTestCount = 0;
    static std::vector<int> faceCounts;
    static std::vector<double> times;
    static std::vector<float> errors;
    static std::chrono::high_resolution_clock::time_point start;
    if (randomTestCount == numTests)
    {
        std::cout << faceCounts.size() << std::endl;
        std::cout << "face counts: " << calculateAverage(faceCounts) << std::endl;
        std::cout << "times: " << calculateAverage(times) << std::endl;
        for (float err : errors)
            std::cout << err << " ";
        std::cout << std::endl;
        appState.mergeProcess = MergeProcess::Merging;
    }
    merger.merge();
    if (appState.mergeMode == NONE && randomTestCount == 0)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    if (appState.mergeMode == NONE)
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        if (elapsed.count() > 0.1)
        {
            merger.metrics.captureGlobalImage(appState.patchRenderParams.glPatches, CURR_IMG);
            appState.mergeError = merger.metrics.evaluateMetric(CURR_IMG, ORIG_IMG);
            std::vector<int> faceIdxs = getValidCompIndices(appState.mesh.getFaces());
            std::cout << randomTestCount << " " << faceIdxs.size() << " " << appState.mergeError << " " << elapsed.count() << std::endl;
            faceCounts.push_back(faceIdxs.size());
            times.push_back(elapsed.count());
            errors.push_back(appState.mergeError);
        }

        randomTestCount++;
        setupNewMesh();
        appState.mergeStatus = NA;
        appState.mergeMode = mode;
        start = end;
    }
}
