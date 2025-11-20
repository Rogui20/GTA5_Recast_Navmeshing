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


int pickedTri = -1;

ViewerApp::ViewerApp()
{
    currentDirectory = std::filesystem::current_path().string();
}

ViewerApp::~ViewerApp()
{
}

bool ViewerApp::buildNavmeshFromCurrentMesh()
{
    if (!loadedMesh)
    {
        printf("[ViewerApp] buildNavmeshFromCurrentMesh: sem mesh carregada.\n");
        return false;
    }

    if (!navData.BuildFromMesh(loadedMesh->navmeshVertices, loadedMesh->indices))
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

    glm::vec3 debugOffset(0.0f);
    if (loadedMesh)
        debugOffset = loadedMesh->renderOffset;

    navData.ExtractDebugMesh(navMeshTris, navMeshLines);

    for (auto& v : navMeshTris)
    {
        v -= debugOffset;
    }

    for (size_t i = 0; i + 1 < navMeshLines.size(); i += 2)
    {
        DebugLine dl;
        dl.x0 = navMeshLines[i].x - debugOffset.x;
        dl.y0 = navMeshLines[i].y - debugOffset.y;
        dl.z0 = navMeshLines[i].z - debugOffset.z;
        dl.x1 = navMeshLines[i+1].x - debugOffset.x;
        dl.y1 = navMeshLines[i+1].y - debugOffset.y;
        dl.z1 = navMeshLines[i+1].z - debugOffset.z;
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
    Mesh* newMesh = ObjLoader::LoadObj(path, centerMesh);
    if (!newMesh)
    {
        printf("Falha ao carregar OBJ!\n");
        return false;
    }

    delete loadedMesh;
    loadedMesh = newMesh;
    pickedTri = -1;

    printf("OBJ carregado: %zu verts, %zu indices\n",
           loadedMesh->renderVertices.size(),
           loadedMesh->indices.size());

    buildNavmeshFromCurrentMesh();

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
            if (!loadedMesh)
                continue;

            int mx = e.button.x;
            int my = e.button.y;

            Ray ray = camera->GetRayFromScreen(mx, my, 1600, 900);

            // MATRIZ MODEL — EXATA usada no render
            glm::mat4 model(1.0f);
            model = glm::translate(model, meshPos);
            model = glm::rotate(model, glm::radians(meshRot.x), glm::vec3(1,0,0));
            model = glm::rotate(model, glm::radians(meshRot.y), glm::vec3(0,1,0));
            model = glm::rotate(model, glm::radians(meshRot.z), glm::vec3(0,0,1));
            model = glm::scale(model, glm::vec3(meshScale));

            float best = FLT_MAX;
            pickedTri = -1;

            for (int i = 0; i < loadedMesh->indices.size(); i += 3)
            {
                int i0 = loadedMesh->indices[i+0];
                int i1 = loadedMesh->indices[i+1];
                int i2 = loadedMesh->indices[i+2];

                // ****** TRANSFORMAR PARA ESPAÇO MUNDIAL ******
                glm::vec3 v0 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i0], 1.0f));
                glm::vec3 v1 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i1], 1.0f));
                glm::vec3 v2 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i2], 1.0f));

                float dist;
                if (RayTriangleIntersect(ray, v0, v1, v2, dist))
                {
                    if (dist < best)
                    {
                        best = dist;
                        pickedTri = i;
                    }
                }
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

    // --- UI: Render Mode ---
    ImGui::Begin("Render Mode");

    int mode = (int)renderMode;
    ImGui::RadioButton("Solid (F1)",     &mode, 0);
    ImGui::RadioButton("Wireframe (F2)", &mode, 1);
    ImGui::RadioButton("Normals (F3)",   &mode, 2);
    ImGui::RadioButton("Depth (F4)",     &mode, 3);
    ImGui::RadioButton("Lit (F5)", &mode, 4);


    renderMode = (RenderMode)mode;

    ImGui::End();

    // --- UI: OBJ Browser ---
    ImGui::Begin("Mesh Browser");
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


    // --- Navmesh Menu ---
    ImGui::Begin("Navmesh Menu");
    bool hasMesh = loadedMesh != nullptr;

    if (!hasMesh)
    {
        ImGui::Text("Nenhuma mesh carregada.");
    }

    ImGui::BeginDisabled(!hasMesh);
    if (ImGui::Button("Rebuild Navmesh"))
    {
        buildNavmeshFromCurrentMesh();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Visibilidade");
    ImGui::Checkbox("Mostrar navmesh", &navmeshVisible);

    int navRenderMode = static_cast<int>(navmeshRenderMode);
    ImGui::Text("Render Mode");
    ImGui::RadioButton("Faces e linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesAndLines));
    ImGui::RadioButton("Somente faces", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesOnly));
    ImGui::RadioButton("Somente linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::LinesOnly));
    navmeshRenderMode = static_cast<NavmeshRenderMode>(navRenderMode);

    ImGui::Checkbox("Mostrar faces da navmesh", &navmeshShowFaces);
    ImGui::BeginDisabled(!navmeshShowFaces || navmeshRenderMode == NavmeshRenderMode::LinesOnly);
    ImGui::SliderFloat("Alpha das faces", &navmeshFaceAlpha, 0.0f, 1.0f);
    ImGui::EndDisabled();
    ImGui::Checkbox("Mostrar linhas da navmesh", &navmeshShowLines);

    ImGui::Separator();
    ImGui::Text("Tris: %zu", navMeshTris.size() / 3);
    ImGui::Text("Linhas: %zu", m_navmeshLines.size());

    ImGui::End();


    // --- Debug Camera ---
    ImGui::Begin("Debug");
    ImGui::Text("Camera: %.2f %.2f %.2f",
        camera->pos.x, camera->pos.y, camera->pos.z);
    ImGui::End();

    // --- OpenGL ---
    glEnable(GL_DEPTH_TEST);
    glClearColor(0,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Begin renderer com renderMode
    renderer->Begin(camera, renderMode);
    glUniform3f(glGetUniformLocation(renderer->GetShader(), "uSolidColor"), 1,1,1);

    // OBJ

    if (loadedMesh)
    {
        ImGui::Begin("Mesh Info");

        ImGui::Text("Navmesh bounds:");
        ImGui::Text("Min: %.1f %.1f %.1f",
            loadedMesh->navmeshMinBounds.x,
            loadedMesh->navmeshMinBounds.y,
            loadedMesh->navmeshMinBounds.z);

        ImGui::Text("Max: %.1f %.1f %.1f",
            loadedMesh->navmeshMaxBounds.x,
            loadedMesh->navmeshMaxBounds.y,
            loadedMesh->navmeshMaxBounds.z);

        glm::vec3 center = (loadedMesh->navmeshMinBounds + loadedMesh->navmeshMaxBounds) * 0.5f;
        ImGui::Text("Center: %.1f %.1f %.1f",
            center.x, center.y, center.z);

        ImGui::Separator();
        ImGui::Text("Render bounds (offset: %.1f %.1f %.1f)",
            loadedMesh->renderOffset.x,
            loadedMesh->renderOffset.y,
            loadedMesh->renderOffset.z);
        ImGui::Text("Min: %.1f %.1f %.1f",
            loadedMesh->renderMinBounds.x,
            loadedMesh->renderMinBounds.y,
            loadedMesh->renderMinBounds.z);

        ImGui::Text("Max: %.1f %.1f %.1f",
            loadedMesh->renderMaxBounds.x,
            loadedMesh->renderMaxBounds.y,
            loadedMesh->renderMaxBounds.z);

        glm::vec3 renderCenter = (loadedMesh->renderMinBounds + loadedMesh->renderMaxBounds) * 0.5f;
        ImGui::Text("Render Center: %.1f %.1f %.1f",
            renderCenter.x, renderCenter.y, renderCenter.z);

        if (ImGui::Button("Teleport Camera to Mesh Center"))
            camera->pos = renderCenter + glm::vec3(0,50,150);

        ImGui::End();

        glm::mat4 model(1.0f);
        model = glm::translate(model, meshPos);
        model = glm::rotate(model, glm::radians(meshRot.x), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(meshRot.y), glm::vec3(0,1,0));
        model = glm::rotate(model, glm::radians(meshRot.z), glm::vec3(0,0,1));
        model = glm::scale(model, glm::vec3(meshScale));

        glUseProgram(renderer->GetShader());
        glUniformMatrix4fv(
            glGetUniformLocation(renderer->GetShader(), "uModel"),
            1, GL_FALSE, &model[0][0]
        );

        loadedMesh->Draw();

        if (pickedTri >= 0)
        {
            if (pickedTri + 2 >= (int)loadedMesh->indices.size())
            {
                pickedTri = -1;
            }
            else
            {
                int i0 = loadedMesh->indices[pickedTri+0];
                int i1 = loadedMesh->indices[pickedTri+1];
                int i2 = loadedMesh->indices[pickedTri+2];

                glm::vec3 v0 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i0], 1.0f));
                glm::vec3 v1 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i1], 1.0f));
                glm::vec3 v2 = glm::vec3(model * glm::vec4(loadedMesh->renderVertices[i2], 1.0f));

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
