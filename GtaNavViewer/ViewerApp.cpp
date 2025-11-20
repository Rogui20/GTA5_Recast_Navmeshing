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
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>


ViewerApp::ViewerApp()
{
    currentDirectory = std::filesystem::current_path().string();
}

ViewerApp::~ViewerApp()
{
}

glm::mat4 ViewerApp::GetModelMatrix(const MeshInstance& instance) const
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, instance.position);
    model = glm::rotate(model, glm::radians(instance.rotation.x), glm::vec3(1,0,0));
    model = glm::rotate(model, glm::radians(instance.rotation.y), glm::vec3(0,1,0));
    model = glm::rotate(model, glm::radians(instance.rotation.z), glm::vec3(0,0,1));
    model = glm::scale(model, glm::vec3(instance.scale));
    return model;
}

bool ViewerApp::buildNavmeshFromMeshes()
{
    if (meshInstances.empty())
    {
        printf("[ViewerApp] buildNavmeshFromMeshes: sem meshes carregadas.\n");
        navMeshTris.clear();
        navMeshLines.clear();
        m_navmeshLines.clear();
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

    if (!navData.BuildFromMesh(combinedVerts, combinedIdx, navGenSettings))
    {
        printf("[ViewerApp] BuildFromMesh falhou.\n");
        return false;
    }

    buildNavmeshDebugLines();
    return true;
}

void ViewerApp::buildNavmeshDebugLines()
{
    navMeshTris.clear();
    navMeshLines.clear();
    m_navmeshLines.clear();

    navData.ExtractDebugMesh(navMeshTris, navMeshLines);

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

bool RayTriangleIntersect(const Ray& r,
                          const glm::vec3& v0,
                          const glm::vec3& v1,
                          const glm::vec3& v2,
                          float& outDist)
{
    const float EPS = 0.00001f;
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;

    glm::vec3 p = glm::cross(r.dir, e2);
    float det = glm::dot(e1, p);

    if (fabs(det) < EPS) return false;

    float inv = 1.0f / det;

    glm::vec3 t = r.origin - v0;

    float u = glm::dot(t, p) * inv;
    if (u < 0 || u > 1) return false;

    glm::vec3 q = glm::cross(t, e1);

    float v = glm::dot(r.dir, q) * inv;
    if (v < 0 || u + v > 1) return false;

    float dist = glm::dot(e2, q) * inv;
    if (dist < 0) return false;

    outDist = dist;
    return true;
}


void ViewerApp::ProcessEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);

        if (e.type == SDL_QUIT)
            running = false;

        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            running = false;

        if (e.type == SDL_KEYDOWN)
        {
            switch (e.key.keysym.sym)
            {
            case SDLK_F1:
                renderMode = RenderMode::Solid;
                break;

            case SDLK_F2:
                renderMode = RenderMode::Wireframe;
                break;

            case SDLK_F3:
                renderMode = RenderMode::Normals;
                break;

            case SDLK_F4:
                renderMode = RenderMode::Depth;
                break;
            case SDLK_F5:
                renderMode = RenderMode::Lit;
                break;

            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            if (meshInstances.empty())
                continue;

            if (pickTriangleMode || buildTileAtMode)
            {
                int mx = e.button.x;
                int my = e.button.y;

                Ray ray = camera->GetRayFromScreen(mx, my, 1600, 900);

                float best = FLT_MAX;
                pickedTri = -1;
                pickedMeshIndex = -1;
                glm::vec3 bestPoint(0.0f);
                bool hasHit = false;

                for (size_t meshIdx = 0; meshIdx < meshInstances.size(); ++meshIdx)
                {
                    const auto& instance = meshInstances[meshIdx];
                    if (!instance.mesh)
                        continue;

                    glm::mat4 model = GetModelMatrix(instance);
                    for (int i = 0; i < instance.mesh->indices.size(); i += 3)
                    {
                        int i0 = instance.mesh->indices[i+0];
                        int i1 = instance.mesh->indices[i+1];
                        int i2 = instance.mesh->indices[i+2];

                        glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
                        glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
                        glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

                        float dist;
                        if (RayTriangleIntersect(ray, v0, v1, v2, dist))
                        {
                            if (dist < best)
                            {
                                best = dist;
                                pickedTri = i;
                                pickedMeshIndex = static_cast<int>(meshIdx);
                                bestPoint = ray.origin + ray.dir * dist;
                                hasHit = true;
                            }
                        }
                    }
                }

                if (buildTileAtMode && hasHit)
                {
                    int tileX = -1;
                    int tileY = -1;
                    if (navData.BuildTileAt(bestPoint, navGenSettings, tileX, tileY))
                    {
                        buildNavmeshDebugLines();
                    }
                    else
                    {
                        printf("[ViewerApp] BuildTileAt falhou para o clique em (%.2f, %.2f, %.2f).\n",
                               bestPoint.x, bestPoint.y, bestPoint.z);
                    }
                }
            }
            else if (addRemoveTileMode)
            {
                printf("[ViewerApp] addRemoveTileMode ainda não implementado.\n");
            }
        }



        // Mouse capture toggle
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
        {
            mouseCaptured = true;
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT)
        {
            mouseCaptured = false;
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        if (mouseCaptured)
        {
            if (e.type == SDL_MOUSEMOTION)
                camera->OnMouseMove(e.motion.xrel, e.motion.yrel);
        }
    }

    const Uint8* state = SDL_GetKeyboardState(NULL);
    camera->Update(state);
}

void ViewerApp::RenderFrame()
{
    // --- Iniciar nova frame ImGui ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

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
            ImGui::BeginDisabled(!hasMesh);
            if (ImGui::MenuItem("Rebuild Navmesh"))
            {
                buildNavmeshFromMeshes();
            }
            ImGui::EndDisabled();

            ImGui::Separator();
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
                    bool pickMode = pickTriangleMode;
                    if (ImGui::Checkbox("pickTriangleMode", &pickMode))
                    {
                        pickTriangleMode = pickMode;
                        if (pickTriangleMode)
                        {
                            buildTileAtMode = false;
                            addRemoveTileMode = false;
                        }
                        else if (!buildTileAtMode && !addRemoveTileMode)
                        {
                            pickTriangleMode = true;
                        }
                    }

                    bool buildTileMode = buildTileAtMode;
                    if (ImGui::Checkbox("buildTileAtMode", &buildTileMode))
                    {
                        buildTileAtMode = buildTileMode;
                        if (buildTileAtMode)
                        {
                            pickTriangleMode = false;
                            addRemoveTileMode = false;
                        }
                        else if (!pickTriangleMode && !addRemoveTileMode)
                        {
                            pickTriangleMode = true;
                        }
                    }

                    ImGui::BeginDisabled(true);
                    bool addRemoveMode = addRemoveTileMode;
                    if (ImGui::Checkbox("addRemoveTileMode (em breve)", &addRemoveMode))
                    {
                        addRemoveTileMode = addRemoveMode;
                        if (addRemoveTileMode)
                        {
                            pickTriangleMode = false;
                            buildTileAtMode = false;
                        }
                    }
                    ImGui::EndDisabled();

                    const char* activeMode = pickTriangleMode ? "pickTriangleMode" : (buildTileAtMode ? "buildTileAtMode" : "addRemoveTileMode");
                    ImGui::Text("Modo ativo: %s", activeMode);
                }

                if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::InputTextWithHint("##meshFilter", "Filtrar por nome", meshFilter, IM_ARRAYSIZE(meshFilter));
                    if (ImGui::BeginTable("MeshTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Mesh");
                        ImGui::TableSetupColumn("Transform");
                        ImGui::TableSetupColumn("Ações", ImGuiTableColumnFlags_WidthFixed, 120.0f);
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

                            ImGui::PushID(static_cast<int>(i));
                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            bool isSelected = static_cast<int>(i) == pickedMeshIndex;
                            if (ImGui::Selectable(instance.name.c_str(), isSelected))
                            {
                                pickedMeshIndex = static_cast<int>(i);
                            }

                            ImGui::TableNextColumn();
                            glm::vec3 previousPos = instance.position;
                            glm::vec3 previousRot = instance.rotation;
                            bool moved = ImGui::DragFloat3("Pos", &instance.position.x, 0.25f);
                            bool rotated = ImGui::DragFloat3("Rot", &instance.rotation.x, 0.25f);

                            if (moved && instance.position != previousPos)
                            {
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnMove);
                            }
                            if (rotated && instance.rotation != previousRot)
                            {
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnRotate);
                            }

                            ImGui::TableNextColumn();
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
                }

                if (ImGui::CollapsingHeader("Navmesh Gen Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
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
                }

                if (ImGui::CollapsingHeader("Navmesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool hasMesh = !meshInstances.empty();
                    ImGui::Text("Meshes carregadas: %zu", meshInstances.size());

                    ImGui::BeginDisabled(!hasMesh);
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

    for (size_t meshIdx = 0; meshIdx < meshInstances.size(); ++meshIdx)
    {
        const auto& instance = meshInstances[meshIdx];
        if (!instance.mesh)
            continue;

        glm::mat4 model = GetModelMatrix(instance);

        glUseProgram(renderer->GetShader());
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

    if (drawFaces)
    {
        float clampedAlpha = std::clamp(navmeshFaceAlpha, 0.0f, 1.0f);
        renderer->drawNavmeshTriangles(navMeshTris, glm::vec3(0.5f, 0.7f, 1.0f), clampedAlpha);
    }

    if (drawLines)
    {
        renderer->drawNavmeshLines(m_navmeshLines);
    }

    // desenhar grid e eixo sempre no modo 99
    glDisable(GL_DEPTH_TEST);   // Gizmos ficam por cima

    renderer->Begin(camera, RenderMode::None); // vai setar modo=99
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
