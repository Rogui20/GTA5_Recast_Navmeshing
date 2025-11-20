// ViewerApp.h
#pragma once
#include <string>
#include <SDL.h>
#include <filesystem>
#include <cstdint>
#include "Mesh.h"
#include "RenderMode.h"
#include "GtaNavAPI.h"
#include "NavMeshData.h"
#include "RendererGL.h"
#include <memory>
#include <vector>

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
    bool buildNavmeshFromMeshes();
    void buildNavmeshDebugLines();

private:
    // SDL
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;

    // Controls
    bool running = true;
    bool mouseCaptured = false;

    struct MeshInstance
    {
        std::unique_ptr<Mesh> mesh;
        std::string           name;
        std::filesystem::path sourcePath;
        glm::vec3             position {0.0f};
        glm::vec3             rotation {0.0f};
        float                 scale = 1.0f;
    };

    // Engine components
    ViewerCamera* camera = nullptr;
    RendererGL* renderer = nullptr;
    std::vector<MeshInstance> meshInstances;
    RenderMode renderMode = RenderMode::Solid;
    bool centerMesh = true;
    NavMeshData navData;
    NavmeshGenerationSettings navGenSettings{};

    enum class NavmeshRenderMode
    {
        FacesAndLines = 0,
        FacesOnly,
        LinesOnly
    };

    bool navmeshVisible = true;
    bool navmeshShowFaces = true;
    bool navmeshShowLines = true;
    float navmeshFaceAlpha = 0.35f;
    NavmeshRenderMode navmeshRenderMode = NavmeshRenderMode::FacesAndLines;

    enum class NavmeshAutoBuildFlag : uint32_t
    {
        None    = 0,
        OnAdd   = 1 << 0,
        OnRemove= 1 << 1,
        OnMove  = 1 << 2,
        OnRotate= 1 << 3,
    };

    uint32_t navmeshAutoBuildMask = 0;

    bool pickTriangleMode = true;
    bool addRemoveTileMode = false;

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

    void RemoveMesh(size_t index);
    void HandleAutoBuild(NavmeshAutoBuildFlag flag);
    glm::mat4 GetModelMatrix(const MeshInstance& instance) const;

    void ProcessEvents();
    void RenderFrame();

    std::string currentDirectory;
    std::string selectedEntry;

    bool showSidePanel = true;
    bool showMeshBrowserWindow = true;
    bool showDebugInfo = true;
    char meshFilter[64] = {0};

    int pickedMeshIndex = -1;
    int pickedTri = -1;
};
