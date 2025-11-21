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
#include <DetourNavMeshQuery.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

struct Ray;
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
    bool buildNavmeshFromMeshes(bool buildTilesNow = true);
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
        uint64_t              id = 0;
    };

    struct MeshBoundsState
    {
        glm::vec3 bmin{0.0f};
        glm::vec3 bmax{0.0f};
        size_t vertexCount = 0;
        size_t indexCount = 0;
        const Mesh* meshPtr = nullptr;
        bool valid = false;
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
    bool navmeshUpdateTiles = false;

    enum class NavmeshAutoBuildFlag : uint32_t
    {
        None    = 0,
        OnAdd   = 1 << 0,
        OnRemove= 1 << 1,
        OnMove  = 1 << 2,
        OnRotate= 1 << 3,
    };

    uint32_t navmeshAutoBuildMask = 0;
    uint64_t nextMeshId = 1;
    std::unordered_map<uint64_t, MeshBoundsState> meshStateCache;

    enum class ViewportClickMode
    {
        PickTriangle = 0,
        BuildTileAt = 1,
        RemoveTileAt = 2,
        Pathfind_Normal = 3,
        Pathfind_MinEdge = 4,
        EditMesh = 5
    };

    enum class MeshEditMode
    {
        Move = 0,
        Rotate = 1
    };

    enum class MoveTransformMode
    {
        GlobalTransform = 0,
        LocalTransform = 1
    };

    enum class GizmoAxis
    {
        None,
        X,
        Y,
        Z
    };

    ViewportClickMode viewportClickMode = ViewportClickMode::PickTriangle;
    float pathfindMinEdgeDistance = 0.0f;
    bool hasPathStart = false;
    bool hasPathTarget = false;
    glm::vec3 pathStart{0.0f};
    glm::vec3 pathTarget{0.0f};
    std::vector<DebugLine> pathLines;
    dtNavMeshQuery* navQuery = nullptr;
    dtQueryFilter pathQueryFilter{};
    bool navQueryReady = false;

    MeshEditMode meshEditMode = MeshEditMode::Move;
    MoveTransformMode moveTransformMode = MoveTransformMode::GlobalTransform;
    GizmoAxis activeGizmoAxis = GizmoAxis::None;
    bool editDragActive = false;
    bool leftMouseDown = false;
    glm::vec3 gizmoGrabStart{};
    glm::vec3 gizmoAxisDir{1.0f, 0.0f, 0.0f};
    glm::vec3 gizmoOrigin{};
    float gizmoStartParam = 0.0f;
    glm::vec3 meshStartPosition{};
    glm::vec3 rotationStartVec{};
    glm::vec3 rotationStartAngles{};

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
    MeshBoundsState ComputeMeshBounds(const MeshInstance& instance) const;
    bool HasMeshChanged(const MeshBoundsState& previous, const MeshBoundsState& current) const;
    void UpdateNavmeshTiles();
    bool ComputeRayMeshHit(int mx, int my, glm::vec3& outPoint, int* outTri, int* outMeshIndex);
    void ResetPathState();
    bool InitNavQueryForCurrentNavmesh();
    void TryRunPathfind();
    bool IsPathfindModeActive() const;
    glm::vec3 NormalizeEuler(glm::vec3 angles) const;
    void NormalizeMeshRotation(MeshInstance& instance) const;
    glm::vec3 ToGtaCoords(const glm::vec3& internal) const;
    glm::vec3 FromGtaCoords(const glm::vec3& gta) const;
    glm::mat3 GetRotationMatrix(const glm::vec3& eulerDegrees) const;
    glm::vec3 GetAxisDirection(const MeshInstance& instance, GizmoAxis axis) const;
    glm::vec3 GetRotationAxisDirection(const MeshInstance& instance, GizmoAxis axis) const;
    bool TryBeginMoveDrag(const Ray& ray, MeshInstance& instance);
    bool TryBeginRotateDrag(const Ray& ray, MeshInstance& instance);
    void UpdateEditDrag(int mouseX, int mouseY);
    void DrawSelectedMeshHighlight();
    void DrawEditGizmo();

    void ProcessEvents();
    void RenderFrame();

    void ProcessNavmeshJob();
    void JoinNavmeshWorker();
    void RebuildDebugLineBuffer();
    bool IsNavmeshJobRunning() const { return navmeshJobRunning.load(); }

    std::string currentDirectory;
    std::string selectedEntry;

    bool showSidePanel = true;
    bool showMeshBrowserWindow = true;
    bool showDebugInfo = true;
    char meshFilter[64] = {0};

    int pickedMeshIndex = -1;
    int pickedTri = -1;
    bool rightButtonDown = false;
    bool rightButtonDragged = false;
    int rightButtonDownX = 0;
    int rightButtonDownY = 0;
    bool drawMeshOutlines = true;

    struct NavmeshJobResult
    {
        bool success = false;
        NavMeshData navData;
        std::vector<glm::vec3> tris;
        std::vector<glm::vec3> lines;
    };

    std::thread navmeshWorker;
    std::atomic<bool> navmeshJobRunning{false};
    std::atomic<bool> navmeshJobCompleted{false};
    bool navmeshJobQueued = false;
    bool navmeshJobQueuedBuildTilesNow = true;
    std::mutex navmeshJobMutex;
    std::unique_ptr<NavmeshJobResult> navmeshJobResult;
};
