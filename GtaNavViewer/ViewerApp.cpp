// ViewerApp.cpp
#include "ViewerApp.h"
#include "ViewerCamera.h"
#include "RendererGL.h"
#include "ObjLoader.h"
#include "Mesh.h"
#include "RenderMode.h"
#include "NavMeshData.h"
#include "GtaNavAPI.h"

#include <glad/glad.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <array>
#include <cmath>


ViewerApp::ViewerApp()
    : directoryBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
{
    currentDirectory = std::filesystem::current_path().string();
    directoryBrowser.SetTitle("Selecionar pasta");
    directoryBrowser.SetDirectory(currentDirectory);
}

ViewerApp::~ViewerApp()
{
    if (navQuery)
    {
        dtFreeNavMeshQuery(navQuery);
        navQuery = nullptr;
    }
}

bool ViewerApp::buildNavmeshFromMeshes(bool buildTilesNow)
{
    if (IsNavmeshJobRunning())
    {
        navmeshJobQueued = true;
        navmeshJobQueuedBuildTilesNow = navmeshJobQueuedBuildTilesNow || buildTilesNow;
        printf("[ViewerApp] buildNavmeshFromMeshes: já existe um build em andamento. Marcando rebuild pendente...\n");
        return false;
    }

    if (meshInstances.empty())
    {
        printf("[ViewerApp] buildNavmeshFromMeshes: sem meshes carregadas.\n");
        navMeshTris.clear();
        navMeshLines.clear();
        m_navmeshLines.clear();
        ResetPathState();
        navQueryReady = false;
        return false;
    }

    std::vector<glm::vec3> combinedVerts;
    std::vector<unsigned int> combinedIdx;
    unsigned int baseIndex = 0;

    for (const auto& instance : meshInstances)
    {
        if (!instance.mesh)
            continue;

        glm::mat4 model = GetModelMatrix(instance);
        for (const auto& v : instance.mesh->renderVertices)
        {
            combinedVerts.push_back(glm::vec3(model * glm::vec4(v, 1.0f)));
        }

        for (auto idx : instance.mesh->indices)
        {
            combinedIdx.push_back(baseIndex + idx);
        }

        baseIndex += static_cast<unsigned int>(instance.mesh->renderVertices.size());
    }

    editDragActive = false;
    activeGizmoAxis = GizmoAxis::None;

    JoinNavmeshWorker();
    navmeshJobResult.reset();
    navmeshJobCompleted.store(false);
    navmeshJobRunning.store(true);
    navmeshJobQueued = false;
    navmeshJobQueuedBuildTilesNow = buildTilesNow;

    const auto settingsCopy = navGenSettings;

    navmeshWorker = std::thread([
        this,
        verts = std::move(combinedVerts),
        idx = std::move(combinedIdx),
        settingsCopy,
        buildTilesNow
    ]() mutable
    {
        auto result = std::make_unique<NavmeshJobResult>();
        if (!verts.empty() && !idx.empty())
        {
            result->success = result->navData.BuildFromMesh(verts, idx, settingsCopy, buildTilesNow);
        }

        if (result->success)
        {
            result->navData.ExtractDebugMesh(result->tris, result->lines);
            printf("[ViewerApp] Navmesh build finalizado em worker.\n");
        }
        else
        {
            printf("[ViewerApp] BuildFromMesh falhou no worker.\n");
        }

        {
            std::lock_guard<std::mutex> lock(navmeshJobMutex);
            navmeshJobResult = std::move(result);
        }

        navmeshJobRunning.store(false);
        navmeshJobCompleted.store(true);
    });

    return true;
}

void ViewerApp::buildNavmeshDebugLines()
{
    navMeshTris.clear();
    navMeshLines.clear();
    m_navmeshLines.clear();

    navData.ExtractDebugMesh(navMeshTris, navMeshLines);
    RebuildDebugLineBuffer();
}

void ViewerApp::RebuildDebugLineBuffer()
{
    m_navmeshLines.clear();

    for (size_t i = 0; i + 1 < navMeshLines.size(); i += 2)
    {
        DebugLine dl;
        dl.x0 = navMeshLines[i].x;
        dl.y0 = navMeshLines[i].y;
        dl.z0 = navMeshLines[i].z;
        dl.x1 = navMeshLines[i+1].x;
        dl.y1 = navMeshLines[i+1].y;
        dl.z1 = navMeshLines[i+1].z;
        m_navmeshLines.push_back(dl);
    }

    printf("[ViewerApp] Linhas de navmesh: %zu segmentos.\n", m_navmeshLines.size());
}

void ViewerApp::DrawSelectedMeshHighlight()
{
    if (viewportClickMode != ViewportClickMode::EditMesh)
        return;

    if (!drawMeshOutlines)
        return;

    if (pickedMeshIndex < 0 || pickedMeshIndex >= static_cast<int>(meshInstances.size()))
        return;

    const auto& instance = meshInstances[pickedMeshIndex];
    if (!instance.mesh)
        return;

    glm::mat4 model = GetModelMatrix(instance);

    std::unordered_set<uint64_t> edgeSet;
    std::vector<DebugLine> outlines;
    outlines.reserve(instance.mesh->indices.size());

    auto addEdge = [&](int ia, int ib)
    {
        int minIdx = std::min(ia, ib);
        int maxIdx = std::max(ia, ib);
        uint64_t key = (static_cast<uint64_t>(minIdx) << 32) | static_cast<uint32_t>(maxIdx);
        if (edgeSet.insert(key).second)
        {
            glm::vec3 a = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[ia], 1.0f));
            glm::vec3 b = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[ib], 1.0f));

            DebugLine l{a.x, a.y, a.z, b.x, b.y, b.z};
            outlines.push_back(l);
        }
    };

    for (size_t i = 0; i + 2 < instance.mesh->indices.size(); i += 3)
    {
        int i0 = instance.mesh->indices[i+0];
        int i1 = instance.mesh->indices[i+1];
        int i2 = instance.mesh->indices[i+2];

        glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
        glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
        glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

        addEdge(i0, i1);
        addEdge(i1, i2);
        addEdge(i2, i0);
    }

    if (!outlines.empty())
    {
        glDisable(GL_DEPTH_TEST);
        renderer->drawNavmeshLines(outlines, glm::vec3(1.0f, 0.5f, 0.0f), 3.0f);
        glEnable(GL_DEPTH_TEST);
    }
}

void ViewerApp::DrawEditGizmo()
{
    if (viewportClickMode != ViewportClickMode::EditMesh)
        return;

    if (pickedMeshIndex < 0 || pickedMeshIndex >= static_cast<int>(meshInstances.size()))
        return;

    const auto& instance = meshInstances[pickedMeshIndex];
    glm::vec3 origin = instance.position;
    float camDist = glm::length(camera->pos - origin);
    float axisLength = 4.0f + camDist * 0.05f;
    float lineWidth = 5.0f;

    glm::vec3 axisX = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::X) : glm::vec3(1,0,0);
    glm::vec3 axisY = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::Y) : glm::vec3(0,1,0);
    glm::vec3 axisZ = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::Z) : glm::vec3(0,0,1);

    auto gizmoColor = [&](GizmoAxis axis, const glm::vec3& baseColor)
    {
        if (activeGizmoAxis == axis)
            return baseColor * 1.2f;
        return baseColor;
    };

    if (meshEditMode == MeshEditMode::Move)
    {
        std::array<std::pair<GizmoAxis, glm::vec3>, 3> axisInfo = {
            std::make_pair(GizmoAxis::X, axisX),
            std::make_pair(GizmoAxis::Y, axisY),
            std::make_pair(GizmoAxis::Z, axisZ)
        };

        for (const auto& axis : axisInfo)
        {
            glm::vec3 dir = glm::normalize(axis.second);
            DebugLine line{origin.x, origin.y, origin.z,
                           origin.x + dir.x * axisLength,
                           origin.y + dir.y * axisLength,
                           origin.z + dir.z * axisLength};

            glm::vec3 color(1,0,0);
            if (axis.first == GizmoAxis::Y) color = glm::vec3(0,1,0);
            else if (axis.first == GizmoAxis::Z) color = glm::vec3(0,0,1);

            renderer->drawNavmeshLines({line}, gizmoColor(axis.first, color), lineWidth);
        }
    }

    if (meshEditMode == MeshEditMode::Rotate)
    {
        float radius = 3.5f + camDist * 0.03f;
        const int segments = 64;

    auto drawCircle = [&](GizmoAxis axis, const glm::vec3& basisA, const glm::vec3& basisB, const glm::vec3& color)
    {
        std::vector<DebugLine> circle;
        circle.reserve(segments);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = (static_cast<float>(i) / segments) * glm::two_pi<float>();
            float a1 = (static_cast<float>(i + 1) / segments) * glm::two_pi<float>();

            glm::vec3 p0 = origin + basisA * std::cos(a0) * radius + basisB * std::sin(a0) * radius;
            glm::vec3 p1 = origin + basisA * std::cos(a1) * radius + basisB * std::sin(a1) * radius;

            circle.push_back({p0.x, p0.y, p0.z, p1.x, p1.y, p1.z});
        }

        renderer->drawNavmeshLines(circle, gizmoColor(axis, color), 3.0f);
    };

        glm::vec3 right = GetAxisDirection(instance, GizmoAxis::X);
        glm::vec3 up    = GetAxisDirection(instance, GizmoAxis::Y);
        glm::vec3 fwd   = GetAxisDirection(instance, GizmoAxis::Z);

        drawCircle(GizmoAxis::X, up, fwd, glm::vec3(1,0,0));
        drawCircle(GizmoAxis::Y, right, fwd, glm::vec3(0,1,0));
        drawCircle(GizmoAxis::Z, glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,1));
    }
}


bool ViewerApp::Init()
{
    if (!InitSDL()) return false;
    if (!InitGL())  return false;
    if (!InitImGui()) return false;

    camera   = new ViewerCamera();
    renderer = new RendererGL();

    LoadLastDirectory();

    return true;
}


void ViewerApp::Shutdown()
{
    JoinNavmeshWorker();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    delete camera;
    delete renderer;
}

bool ViewerApp::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return false;
    }

    // OpenGL 3.3 Core
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);    

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);


    window = SDL_CreateWindow(
        "GTA NavMesh Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1600, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window)
    {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // vsync
    printf("window = %p\n", window);
    printf("glContext = %p\n", glContext);

    return true;
}

bool ViewerApp::InitGL()
{
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_Log("Failed to initialize GLAD");
        return false;
    }

    glViewport(0, 0, 1600, 900);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LESS); // já deve ser default, mas garantimos

    return true;
}

bool ViewerApp::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // POR ENQUANTO: sem docking nem viewports
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

bool ViewerApp::LoadMeshFromPath(const std::string& path)
{ 
    std::unique_ptr<Mesh> newMesh(ObjLoader::LoadObj(path, centerMesh));
    if (!newMesh)
    {
        printf("Falha ao carregar OBJ!\n");
        return false;
    }

    MeshInstance instance;
    instance.name = std::filesystem::path(path).stem().string();
    instance.sourcePath = path;
    instance.id = nextMeshId++;
    instance.mesh = std::move(newMesh);
    meshInstances.push_back(std::move(instance));
    pickedTri = -1;
    pickedMeshIndex = -1;

    printf("OBJ carregado: %zu verts, %zu indices\n",
           meshInstances.back().mesh->renderVertices.size(),
           meshInstances.back().mesh->indices.size());

    HandleAutoBuild(NavmeshAutoBuildFlag::OnAdd);

    std::filesystem::path p(path);
    SaveLastDirectory(p.parent_path());

    return true;
}

std::filesystem::path ViewerApp::GetConfigFilePath() const
{
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::filesystem::path result = std::filesystem::path(basePath) / "last_directory.txt";
        SDL_free(basePath);
        return result;
    }

    return "last_directory.txt";
}

void ViewerApp::LoadLastDirectory()
{
    std::filesystem::path configPath = GetConfigFilePath();
    std::ifstream file(configPath);
    if (file)
    {
        std::string storedPath;
        std::getline(file, storedPath);
        if (!storedPath.empty() && std::filesystem::exists(storedPath))
        {
            currentDirectory = storedPath;
        }
    }
}

void ViewerApp::SaveLastDirectory(const std::filesystem::path& directory)
{
    if (directory.empty()) return;

    std::filesystem::path configPath = GetConfigFilePath();
    std::ofstream file(configPath, std::ios::trunc);
    if (file)
    {
        file << directory.string();
    }
}

void ViewerApp::HandleAutoBuild(NavmeshAutoBuildFlag flag)
{
    if (flag == NavmeshAutoBuildFlag::None)
        return;

    if (static_cast<bool>(navmeshAutoBuildMask & static_cast<uint32_t>(flag)))
    {
        buildNavmeshFromMeshes();
    }
}


void ViewerApp::RemoveMesh(size_t index)
{
    if (index >= meshInstances.size())
        return;

    meshInstances.erase(meshInstances.begin() + index);
    if (pickedMeshIndex == static_cast<int>(index))
    {
        pickedMeshIndex = -1;
        pickedTri = -1;
    }
    else if (pickedMeshIndex > static_cast<int>(index))
    {
        pickedMeshIndex--;
    }

    HandleAutoBuild(NavmeshAutoBuildFlag::OnRemove);

    if (meshInstances.empty())
    {
        navMeshTris.clear();
        navMeshLines.clear();
        m_navmeshLines.clear();
        ResetPathState();
        navQueryReady = false;
    }
}


void ViewerApp::Run()
{
    while (running)
    {
        ProcessEvents();
        RenderFrame();
    }
}

void ViewerApp::JoinNavmeshWorker()
{
    if (navmeshWorker.joinable())
    {
        navmeshWorker.join();
    }
}

void ViewerApp::ProcessNavmeshJob()
{
    if (!navmeshJobCompleted.load())
        return;

    JoinNavmeshWorker();
    navmeshJobCompleted.store(false);

    std::unique_ptr<NavmeshJobResult> result;
    {
        std::lock_guard<std::mutex> lock(navmeshJobMutex);
        result = std::move(navmeshJobResult);
    }

    if (result && result->success)
    {
        navData = std::move(result->navData);
        navMeshTris = std::move(result->tris);
        navMeshLines = std::move(result->lines);
        RebuildDebugLineBuffer();
        navQueryReady = false;

        const bool queryOk = InitNavQueryForCurrentNavmesh();
        if (queryOk && hasPathStart && hasPathTarget)
        {
            TryRunPathfind();
        }
        else if (!queryOk)
        {
            ResetPathState();
        }

        printf("[ViewerApp] Navmesh worker aplicado na thread principal.\n");
    }
    else if (result)
    {
        printf("[ViewerApp] Navmesh worker falhou. Mantendo navmesh atual.\n");
    }

    if (navmeshJobQueued)
    {
        bool buildTilesNow = navmeshJobQueuedBuildTilesNow;
        navmeshJobQueued = false;
        buildNavmeshFromMeshes(buildTilesNow);
    }
}





void ViewerApp::RenderFrame()
{
    ProcessNavmeshJob();

    // --- Iniciar nova frame ImGui ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (!IsNavmeshJobRunning())
        UpdateNavmeshTiles();

    const bool navmeshBusy = IsNavmeshJobRunning();

    if (viewportClickMode == ViewportClickMode::EditMesh && editDragActive && leftMouseDown)
    {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        UpdateEditDrag(mx, my);
    }
    else if (!leftMouseDown && editDragActive)
    {
        editDragActive = false;
        activeGizmoAxis = GizmoAxis::None;
    }

    // --- Barra de menus principal ---
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Arquivo"))
        {
            ImGui::MenuItem("Browser de Meshes", nullptr, &showMeshBrowserWindow);
            ImGui::MenuItem("Painel Lateral", nullptr, &showSidePanel);
            ImGui::Separator();
            ImGui::MenuItem("Centralizar mesh carregada", nullptr, &centerMesh);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Visualização"))
        {
            ImGui::TextDisabled("Render Mode");
            int mode = static_cast<int>(renderMode);
            ImGui::RadioButton("Solid (F1)", &mode, 0);
            ImGui::RadioButton("Wireframe (F2)", &mode, 1);
            ImGui::RadioButton("Normals (F3)", &mode, 2);
            ImGui::RadioButton("Depth (F4)", &mode, 3);
            ImGui::RadioButton("Lit (F5)", &mode, 4);
            renderMode = static_cast<RenderMode>(mode);

            ImGui::Separator();
            ImGui::MenuItem("Mostrar navmesh", nullptr, &navmeshVisible);
            ImGui::MenuItem("Mostrar debug/câmera", nullptr, &showDebugInfo);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Navmesh"))
        {
            bool hasMesh = !meshInstances.empty();
            ImGui::BeginDisabled(!hasMesh || navmeshBusy);
            if (ImGui::MenuItem("Rebuild Navmesh"))
            {
                buildNavmeshFromMeshes();
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::BeginDisabled(navmeshBusy);
            bool autoOnAdd    = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd)) != 0;
            bool autoOnRemove = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove)) != 0;
            bool autoOnMove   = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove)) != 0;
            bool autoOnRotate = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate)) != 0;

            if (ImGui::MenuItem("Rebuild ao adicionar", nullptr, &autoOnAdd))
            {
                if (autoOnAdd)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
            }
            if (ImGui::MenuItem("Rebuild ao remover", nullptr, &autoOnRemove))
            {
                if (autoOnRemove)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
            }
            if (ImGui::MenuItem("Rebuild ao mover", nullptr, &autoOnMove))
            {
                if (autoOnMove)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
            }
            if (ImGui::MenuItem("Rebuild ao rotacionar", nullptr, &autoOnRotate))
            {
                if (autoOnRotate)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
            }
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ajuda"))
        {
            ImGui::MenuItem("Debug/Diagnóstico", nullptr, &showDebugInfo);
            ImGui::TextDisabled("Use TABs no painel lateral para alternar modos.");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- OBJ Browser ---
    if (showMeshBrowserWindow)
    {
        ImGui::Begin("Mesh Browser", &showMeshBrowserWindow);
        if (!std::filesystem::exists(currentDirectory))
        {
            currentDirectory = std::filesystem::current_path().string();
        }

        ImGui::Text("Diretório: %s", currentDirectory.c_str());
        ImGui::Checkbox("Centralizar mesh carregada", &centerMesh);

        if (ImGui::Button("Selecionar pasta"))
        {
            directoryBrowser.SetDirectory(currentDirectory);
            directoryBrowser.Open();
        }

        if (ImGui::Button("Subir"))
        {
            std::filesystem::path parent = std::filesystem::path(currentDirectory).parent_path();
            if (!parent.empty())
            {
                currentDirectory = parent.string();
                selectedEntry.clear();
            }
        }

        std::error_code ec;
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> objFiles;
        for (const auto& entry : std::filesystem::directory_iterator(currentDirectory, ec))
        {
            if (entry.is_directory())
            {
                directories.push_back(entry);
            }
            else
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (ext == ".obj")
                    objFiles.push_back(entry);
            }
        }

        std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b){ return a.path().filename() < b.path().filename(); });
        std::sort(objFiles.begin(), objFiles.end(), [](const auto& a, const auto& b){ return a.path().filename() < b.path().filename(); });

        ImGui::SeparatorText("Pastas");
        for (const auto& dir : directories)
        {
            std::string label = "[DIR] " + dir.path().filename().string();
            bool selected = selectedEntry == dir.path().string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedEntry = dir.path().string();
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    currentDirectory = dir.path().string();
                    selectedEntry.clear();
                }
            }
        }

        ImGui::SeparatorText("Arquivos OBJ");
        for (const auto& file : objFiles)
        {
            std::string label = file.path().filename().string();
            bool selected = selectedEntry == file.path().string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedEntry = file.path().string();
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    LoadMeshFromPath(selectedEntry);
                }
            }
        }

        if (!ec)
        {
            if (ImGui::Button("Carregar OBJ selecionado"))
            {
                std::filesystem::path p(selectedEntry);
                auto ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (ext == ".obj")
                {
                    LoadMeshFromPath(selectedEntry);
                }
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(1,0,0,1), "Erro ao listar diretório: %s", ec.message().c_str());
        }

        directoryBrowser.Display();
        if (directoryBrowser.HasSelected())
        {
            currentDirectory = directoryBrowser.GetSelected().string();
            selectedEntry.clear();
            directoryBrowser.ClearSelected();
        }

        ImGui::End();
    }

    // --- Painel lateral com tabs ---
    if (showSidePanel)
    {
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Painel", &showSidePanel, ImGuiWindowFlags_NoCollapse);

        if (ImGui::BeginTabBar("PainelTabs"))
        {
            if (ImGui::BeginTabItem("Edição"))
            {
                if (ImGui::CollapsingHeader("Renderização", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int mode = static_cast<int>(renderMode);
                    ImGui::RadioButton("Solid (F1)", &mode, 0);
                    ImGui::RadioButton("Wireframe (F2)", &mode, 1);
                    ImGui::RadioButton("Normals (F3)", &mode, 2);
                    ImGui::RadioButton("Depth (F4)", &mode, 3);
                    ImGui::RadioButton("Lit (F5)", &mode, 4);
                    renderMode = static_cast<RenderMode>(mode);
                }

                if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("Comportamento do clique");
                    ViewportClickMode previousMode = viewportClickMode;
                    int viewportModeValue = static_cast<int>(viewportClickMode);

                    ImGui::RadioButton("pickTriangleMode", &viewportModeValue, static_cast<int>(ViewportClickMode::PickTriangle));
                    ImGui::RadioButton("buildTileAtMode", &viewportModeValue, static_cast<int>(ViewportClickMode::BuildTileAt));
                    ImGui::RadioButton("removeTileMode", &viewportModeValue, static_cast<int>(ViewportClickMode::RemoveTileAt));
                    ImGui::RadioButton("Pathfind", &viewportModeValue, static_cast<int>(ViewportClickMode::Pathfind_Normal));
                    ImGui::RadioButton("Pathfind With Min Edge Distance", &viewportModeValue, static_cast<int>(ViewportClickMode::Pathfind_MinEdge));
                    ImGui::RadioButton("Edit Mesh", &viewportModeValue, static_cast<int>(ViewportClickMode::EditMesh));

                    viewportClickMode = static_cast<ViewportClickMode>(viewportModeValue);
                    ImGui::BeginDisabled(viewportClickMode != ViewportClickMode::Pathfind_MinEdge);
                    ImGui::SliderFloat("Min Edge Distance", &pathfindMinEdgeDistance, 0.0f, 100.0f, "%.2f");
                    ImGui::EndDisabled();

                    if (viewportClickMode != ViewportClickMode::EditMesh)
                    {
                        editDragActive = false;
                        activeGizmoAxis = GizmoAxis::None;
                        if (previousMode == ViewportClickMode::EditMesh)
                        {
                            pickedMeshIndex = -1;
                            pickedTri = -1;
                        }
                    }

                    if (viewportClickMode == ViewportClickMode::EditMesh)
                    {
                        int editModeValue = static_cast<int>(meshEditMode);
                        ImGui::SeparatorText("Mesh Edit Mode");
                        ImGui::RadioButton("Move", &editModeValue, static_cast<int>(MeshEditMode::Move));
                        ImGui::RadioButton("Rotate", &editModeValue, static_cast<int>(MeshEditMode::Rotate));
                        meshEditMode = static_cast<MeshEditMode>(editModeValue);

                        ImGui::Checkbox("Draw Mesh Outlines", &drawMeshOutlines);

                        if (meshEditMode == MeshEditMode::Move)
                        {
                            int moveMode = static_cast<int>(moveTransformMode);
                            ImGui::RadioButton("Global", &moveMode, static_cast<int>(MoveTransformMode::GlobalTransform));
                            ImGui::RadioButton("Local", &moveMode, static_cast<int>(MoveTransformMode::LocalTransform));
                            moveTransformMode = static_cast<MoveTransformMode>(moveMode);
                        }
                    }

                    if (!IsPathfindModeActive() && (previousMode == ViewportClickMode::Pathfind_Normal || previousMode == ViewportClickMode::Pathfind_MinEdge))
                    {
                        ResetPathState();
                    }

                    const char* activeMode = "";
                    switch (viewportClickMode)
                    {
                    case ViewportClickMode::PickTriangle: activeMode = "pickTriangleMode"; break;
                    case ViewportClickMode::BuildTileAt: activeMode = "buildTileAtMode"; break;
                    case ViewportClickMode::RemoveTileAt: activeMode = "removeTileMode"; break;
                    case ViewportClickMode::Pathfind_Normal: activeMode = "Pathfind"; break;
                    case ViewportClickMode::Pathfind_MinEdge: activeMode = "Pathfind With Min Edge Distance"; break;
                    case ViewportClickMode::EditMesh: activeMode = "Edit Mesh"; break;
                    }
                    
                    ImGui::Text("Modo ativo: %s", activeMode);
                    ImGui::Text("Start: %s | Target: %s", hasPathStart ? "definido" : "pendente", hasPathTarget ? "definido" : "pendente");
                }

                if (ImGui::CollapsingHeader("Navmesh Tests"))
                {
                    ImGui::BeginDisabled(navmeshBusy);
                    bool updateTiles = navmeshUpdateTiles;
                    if (ImGui::Checkbox("Update Tiles", &updateTiles))
                    {
                        navmeshUpdateTiles = updateTiles;
                        meshStateCache.clear();
                        if (navmeshUpdateTiles)
                        {
                            for (const auto& instance : meshInstances)
                            {
                                MeshBoundsState state = ComputeMeshBounds(instance);
                                if (state.valid)
                                {
                                    meshStateCache[instance.id] = state;
                                }
                            }
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(reconstroi tiles com geometria alterada)");

                    if (ImGui::Button("experimental_buildSingleTile"))
                    {
                        if (navGenSettings.mode != NavmeshBuildMode::Tiled)
                        {
                            printf("[ViewerApp] experimental_buildSingleTile: forçando modo tiled.\n");
                            navGenSettings.mode = NavmeshBuildMode::Tiled;
                        }

                        if (buildNavmeshFromMeshes(false))
                        {
                            if (IsPathfindModeActive())
                                ResetPathState();
                            viewportClickMode = ViewportClickMode::BuildTileAt;
                            printf("[ViewerApp] Navmesh tiled inicializado sem tiles. Clique na mesh para gerar tiles individuais.\n");
                        }
                        else
                        {
                            printf("[ViewerApp] experimental_buildSingleTile falhou ao preparar navmesh tiled.\n");
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(prepara grid/caches e gera tiles no clique)");
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::BeginDisabled(navmeshBusy);
                    ImGui::InputTextWithHint("##meshFilter", "Filtrar por nome", meshFilter, IM_ARRAYSIZE(meshFilter));
                    if (ImGui::BeginTable("MeshTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Mesh");
                        ImGui::TableSetupColumn("Transform");
                        ImGui::TableSetupColumn("Ações", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i = 0; i < meshInstances.size(); )
                        {
                            auto& instance = meshInstances[i];
                            std::string lowerName = instance.name;
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                            std::string lowerFilter = meshFilter;
                            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), [](unsigned char c){ return (char)std::tolower(c); });

                            if (!lowerFilter.empty() && lowerName.find(lowerFilter) == std::string::npos)
                            {
                                ++i;
                                continue;
                            }

                            bool allowMeshSelection = viewportClickMode == ViewportClickMode::EditMesh;

                            ImGui::PushID(static_cast<int>(i));
                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            bool isSelected = allowMeshSelection && static_cast<int>(i) == pickedMeshIndex;
                            ImGui::BeginDisabled(!allowMeshSelection);
                            if (ImGui::Selectable(instance.name.c_str(), isSelected))
                            {
                                pickedMeshIndex = static_cast<int>(i);
                                pickedTri = -1;
                            }
                            ImGui::EndDisabled();

                            ImGui::TableNextColumn();
                            glm::vec3 previousPos = instance.position;
                            glm::vec3 previousRot = instance.rotation;
                            glm::vec3 gtaPos = ToGtaCoords(instance.position);
                            glm::vec3 previousGtaPos = ToGtaCoords(previousPos);
                            bool moved = ImGui::DragFloat3("Pos", &gtaPos.x, 0.25f);
                            bool rotated = ImGui::DragFloat3("Rot", &instance.rotation.x, 0.25f);
                            if (rotated)
                            {
                                NormalizeMeshRotation(instance);
                            }

                            if (moved && gtaPos != previousGtaPos)
                            {
                                instance.position = FromGtaCoords(gtaPos);
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnMove);
                            }
                            if (rotated && instance.rotation != previousRot)
                            {
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnRotate);
                            }

                            ImGui::TableNextColumn();
                            ImGui::BeginDisabled(viewportClickMode != ViewportClickMode::EditMesh);
                            if (ImGui::Button("Selecionar"))
                            {
                                pickedMeshIndex = static_cast<int>(i);
                                pickedTri = -1;
                            }
                            ImGui::EndDisabled();
                            ImGui::SameLine();

                            if (ImGui::Button("Remover"))
                            {
                                ImGui::PopID();
                                RemoveMesh(i);
                                if (meshInstances.size() <= i)
                                    break;
                                continue;
                            }

                            ImGui::PopID();
                            ++i;
                        }

                        ImGui::EndTable();
                    }

                    if (meshInstances.empty())
                    {
                        ImGui::TextDisabled("Nenhuma mesh carregada.");
                    }
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Navmesh Gen Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::BeginDisabled(navmeshBusy);
                    int buildMode = navGenSettings.mode == NavmeshBuildMode::Tiled ? 1 : 0;
                    if (ImGui::RadioButton("Navmesh normal (single)", buildMode == 0))
                    {
                        navGenSettings.mode = NavmeshBuildMode::SingleMesh;
                    }
                    if (ImGui::RadioButton("Tile based", buildMode == 1))
                    {
                        navGenSettings.mode = NavmeshBuildMode::Tiled;
                    }

                    ImGui::SeparatorText("Voxelização");
                    ImGui::DragFloat("Cell Size", &navGenSettings.cellSize, 0.01f, 0.01f, 5.0f, "%.3f");
                    ImGui::DragFloat("Cell Height", &navGenSettings.cellHeight, 0.01f, 0.01f, 5.0f, "%.3f");

                    ImGui::SeparatorText("Agente");
                    ImGui::DragFloat("Altura", &navGenSettings.agentHeight, 0.05f, 0.1f, 20.0f, "%.2f");
                    ImGui::DragFloat("Raio", &navGenSettings.agentRadius, 0.05f, 0.05f, 10.0f, "%.2f");
                    ImGui::DragFloat("Climb", &navGenSettings.agentMaxClimb, 0.05f, 0.0f, 10.0f, "%.2f");
                    ImGui::DragFloat("Slope", &navGenSettings.agentMaxSlope, 1.0f, 0.0f, 89.0f, "%.1f");

                    ImGui::SeparatorText("Polígonos e Regiões");
                    ImGui::DragFloat("Max Edge Len", &navGenSettings.maxEdgeLen, 0.5f, 0.1f, 200.0f, "%.2f");
                    ImGui::DragFloat("Simplification Error", &navGenSettings.maxSimplificationError, 0.01f, 0.0f, 10.0f, "%.2f");
                    ImGui::DragFloat("Min Region Size", &navGenSettings.minRegionSize, 0.5f, 0.0f, 200.0f, "%.1f");
                    ImGui::DragFloat("Merge Region Size", &navGenSettings.mergeRegionSize, 0.5f, 0.0f, 400.0f, "%.1f");
                    ImGui::DragInt("Max Verts/Poly", &navGenSettings.maxVertsPerPoly, 1, 3, 12);

                    ImGui::SeparatorText("Detalhamento");
                    ImGui::DragFloat("Detail Sample Dist", &navGenSettings.detailSampleDist, 0.25f, 0.0f, 100.0f, "%.2f");
                    ImGui::DragFloat("Detail Sample Max Error", &navGenSettings.detailSampleMaxError, 0.1f, 0.0f, 50.0f, "%.2f");

                    ImGui::BeginDisabled(navGenSettings.mode != NavmeshBuildMode::Tiled);
                    ImGui::DragInt("Tile Size (cells)", &navGenSettings.tileSize, 1, 1, 512);
                    ImGui::EndDisabled();

                    navGenSettings.maxVertsPerPoly = std::max(3, navGenSettings.maxVertsPerPoly);
                    navGenSettings.tileSize = std::max(1, navGenSettings.tileSize);
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Navmesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool hasMesh = !meshInstances.empty();
                    ImGui::Text("Meshes carregadas: %zu", meshInstances.size());

                    if (navmeshBusy)
                    {
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Navmesh build em andamento... câmera e pathfind continuam ativos.");
                    }

                    ImGui::BeginDisabled(!hasMesh || navmeshBusy);
                    if (ImGui::Button("Rebuild Navmesh"))
                    {
                        buildNavmeshFromMeshes();
                    }
                    ImGui::EndDisabled();

                    ImGui::SeparatorText("Visibilidade");
                    ImGui::Checkbox("Mostrar navmesh", &navmeshVisible);
                    int navRenderMode = static_cast<int>(navmeshRenderMode);
                    ImGui::RadioButton("Faces e linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesAndLines));
                    ImGui::RadioButton("Somente faces", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesOnly));
                    ImGui::RadioButton("Somente linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::LinesOnly));
                    navmeshRenderMode = static_cast<NavmeshRenderMode>(navRenderMode);

                    ImGui::Checkbox("Mostrar faces da navmesh", &navmeshShowFaces);
                    ImGui::BeginDisabled(!navmeshShowFaces || navmeshRenderMode == NavmeshRenderMode::LinesOnly);
                    ImGui::SliderFloat("Alpha das faces", &navmeshFaceAlpha, 0.0f, 1.0f);
                    ImGui::EndDisabled();
                    ImGui::Checkbox("Mostrar linhas da navmesh", &navmeshShowLines);

                    ImGui::SeparatorText("Construção Automática");
                    ImGui::BeginDisabled(navmeshBusy);
                    bool autoOnAdd    = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd)) != 0;
                    bool autoOnRemove = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove)) != 0;
                    bool autoOnMove   = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove)) != 0;
                    bool autoOnRotate = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate)) != 0;

                    if (ImGui::Checkbox("Ao adicionar Mesh", &autoOnAdd))
                    {
                        if (autoOnAdd)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                    }

                    if (ImGui::Checkbox("Ao remover Mesh", &autoOnRemove))
                    {
                        if (autoOnRemove)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                    }

                    if (ImGui::Checkbox("Ao mover mesh", &autoOnMove))
                    {
                        if (autoOnMove)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                    }

                    if (ImGui::Checkbox("Ao rotacionar mesh", &autoOnRotate))
                    {
                        if (autoOnRotate)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                    }
                    ImGui::EndDisabled();

                    ImGui::Separator();
                    ImGui::Text("Tris: %zu", navMeshTris.size() / 3);
                    ImGui::Text("Linhas: %zu", m_navmeshLines.size());
                }

                ImGui::EndTabItem();
            }

            if (showDebugInfo && ImGui::BeginTabItem("Diagnóstico"))
            {
                const MeshInstance* infoInstance = nullptr;
                if (!meshInstances.empty())
                {
                    if (pickedMeshIndex >= 0 && pickedMeshIndex < static_cast<int>(meshInstances.size()))
                        infoInstance = &meshInstances[pickedMeshIndex];
                    else
                        infoInstance = &meshInstances.front();
                }

                if (ImGui::CollapsingHeader("Mesh Info", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (infoInstance && infoInstance->mesh)
                    {
                        ImGui::Text("Mesh: %s", infoInstance->name.c_str());
                        ImGui::Text("Navmesh bounds:");
                        ImGui::Text("Min: %.1f %.1f %.1f",
                            infoInstance->mesh->navmeshMinBounds.x,
                            infoInstance->mesh->navmeshMinBounds.y,
                            infoInstance->mesh->navmeshMinBounds.z);

                        ImGui::Text("Max: %.1f %.1f %.1f",
                            infoInstance->mesh->navmeshMaxBounds.x,
                            infoInstance->mesh->navmeshMaxBounds.y,
                            infoInstance->mesh->navmeshMaxBounds.z);

                        glm::vec3 center = (infoInstance->mesh->navmeshMinBounds + infoInstance->mesh->navmeshMaxBounds) * 0.5f;
                        ImGui::Text("Center: %.1f %.1f %.1f",
                            center.x, center.y, center.z);

                        ImGui::Separator();
                        ImGui::Text("Render bounds (offset: %.1f %.1f %.1f)",
                            infoInstance->mesh->renderOffset.x,
                            infoInstance->mesh->renderOffset.y,
                            infoInstance->mesh->renderOffset.z);
                        ImGui::Text("Min: %.1f %.1f %.1f",
                            infoInstance->mesh->renderMinBounds.x,
                            infoInstance->mesh->renderMinBounds.y,
                            infoInstance->mesh->renderMinBounds.z);

                        ImGui::Text("Max: %.1f %.1f %.1f",
                            infoInstance->mesh->renderMaxBounds.x,
                            infoInstance->mesh->renderMaxBounds.y,
                            infoInstance->mesh->renderMaxBounds.z);

                        glm::vec3 renderCenter = (infoInstance->mesh->renderMinBounds + infoInstance->mesh->renderMaxBounds) * 0.5f;
                        ImGui::Text("Render Center: %.1f %.1f %.1f",
                            renderCenter.x, renderCenter.y, renderCenter.z);

                        if (ImGui::Button("Teleport Camera to Mesh Center"))
                            camera->pos = renderCenter + glm::vec3(0,50,150);
                    }
                    else
                    {
                        ImGui::TextDisabled("Nenhuma mesh selecionada.");
                    }
                }

                if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("Camera: %.2f %.2f %.2f",
                        camera->pos.x, camera->pos.y, camera->pos.z);
                    ImGui::Text("Meshes carregadas: %zu", meshInstances.size());
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    // --- OpenGL ---
    glEnable(GL_DEPTH_TEST);
    glClearColor(0,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Begin renderer com renderMode
    renderer->Begin(camera, renderMode);
    glUniform3f(glGetUniformLocation(renderer->GetShader(), "uSolidColor"), 1,1,1);

    glm::vec3 defaultBaseColor = renderMode == RenderMode::Lit ?
        glm::vec3(0.8f, 0.8f, 0.9f) : glm::vec3(0.72f, 0.76f, 0.82f);

    for (size_t meshIdx = 0; meshIdx < meshInstances.size(); ++meshIdx)
    {
        const auto& instance = meshInstances[meshIdx];
        if (!instance.mesh)
            continue;

        bool isSelected = viewportClickMode == ViewportClickMode::EditMesh &&
            static_cast<int>(meshIdx) == pickedMeshIndex;

        glm::mat4 model = GetModelMatrix(instance);

        glUseProgram(renderer->GetShader());
        glm::vec3 meshColor = isSelected ? glm::vec3(1.0f, 0.6f, 0.2f) : defaultBaseColor;
        glUniform3f(glGetUniformLocation(renderer->GetShader(), "uBaseColor"), meshColor.x, meshColor.y, meshColor.z);
        glUniformMatrix4fv(
            glGetUniformLocation(renderer->GetShader(), "uModel"),
            1, GL_FALSE, &model[0][0]
        );

        instance.mesh->Draw();

        if (pickedTri >= 0 && pickedMeshIndex == static_cast<int>(meshIdx))
        {
            if (pickedTri + 2 >= (int)instance.mesh->indices.size())
            {
                pickedTri = -1;
            }
            else
            {
                int i0 = instance.mesh->indices[pickedTri+0];
                int i1 = instance.mesh->indices[pickedTri+1];
                int i2 = instance.mesh->indices[pickedTri+2];

                glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
                glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
                glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

                renderer->DrawTriangleHighlight(v0, v1, v2);
            }
        }
    }

    bool drawFaces = navmeshVisible && navmeshShowFaces &&
        navmeshRenderMode != NavmeshRenderMode::LinesOnly &&
        !navMeshTris.empty();
    bool drawLines = navmeshVisible && navmeshShowLines &&
        navmeshRenderMode != NavmeshRenderMode::FacesOnly &&
        !m_navmeshLines.empty();
    bool drawPath = !pathLines.empty();

    if (drawFaces)
    {
        float clampedAlpha = std::clamp(navmeshFaceAlpha, 0.0f, 1.0f);
        renderer->drawNavmeshTriangles(navMeshTris, glm::vec3(0.5f, 0.7f, 1.0f), clampedAlpha);
    }

    if (drawLines)
    {
        renderer->drawNavmeshLines(m_navmeshLines);
    }

    if (drawPath)
    {
        renderer->drawNavmeshLines(pathLines, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    DrawSelectedMeshHighlight();

    // desenhar grid e eixo sempre no modo 99
    glDisable(GL_DEPTH_TEST);   // Gizmos ficam por cima

    renderer->Begin(camera, RenderMode::None); // vai setar modo=99
    DrawEditGizmo();
    renderer->DrawGrid();
    renderer->DrawAxisGizmoScreen(camera, 1600, 900);
    renderer->End();

    glEnable(GL_DEPTH_TEST);


    renderer->End();

    // --- Finalizar ImGui ---
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}
