// ViewerApp.h
#pragma once
#include <string>
#include <SDL.h>
#include <filesystem>
#include "Mesh.h"
#include "RenderMode.h"
#include "GtaNavAPI.h"
#include "NavMeshData.h"
#include "RendererGL.h"

class ViewerCamera;
class RendererGL;


class ViewerApp
{
public:
    ViewerApp();
    ~ViewerApp();

    bool Init();
    void Run();
    void Shutdown();
    bool buildNavmeshFromCurrentMesh();
    void buildNavmeshDebugLines();

private:
    // SDL
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;

    // Controls
    bool running = true;
    bool mouseCaptured = false;

    // Engine components
    ViewerCamera* camera = nullptr;
    RendererGL* renderer = nullptr;
    Mesh* loadedMesh = nullptr;
    RenderMode renderMode = RenderMode::Solid;
    glm::vec3 meshPos = glm::vec3(0, 0, -0);
    glm::vec3 meshRot = glm::vec3(0);
    float     meshScale = 1.0f;
    NavMeshData navData;

    // buffers para desenhar navmesh no renderer
    std::vector<glm::vec3> navMeshTris;
    std::vector<glm::vec3> navMeshLines;
    std::vector<DebugLine> m_navmeshLines; // apenas isso por enquanto



private:
    bool InitSDL();
    bool InitGL();
    bool InitImGui();

    bool LoadMeshFromPath(const std::string& path);
    void LoadLastDirectory();
    void SaveLastDirectory(const std::filesystem::path& directory);
    std::filesystem::path GetConfigFilePath() const;

    void ProcessEvents();
    void RenderFrame();

    std::string currentDirectory;
    std::string selectedEntry;
};
