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
#include "GtaHandler.h"
#include "GtaHandlerMenu.h"
#include "MemoryHandler.h"
#include "WebSockets.h"
#include "imfilebrowser.h"
#include <DetourNavMeshQuery.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <mutex>
#include <array>

struct Ray;
class ViewerCamera;
class RendererGL;
class GtaHandler;
class GtaHandlerMenu;


class ViewerApp
{
public:
    ViewerApp();
    ~ViewerApp();

    bool Init();
    void Run();
    void Shutdown();
    bool buildNavmeshFromMeshes(bool buildTilesNow = true, int slotIndex = -1);
    void buildNavmeshDebugLines(int slotIndex = -1);
    bool BuildStaticMap(GtaHandler& handler, const std::filesystem::path& meshDirectory, float scanRange);
    void SetProceduralTestEnabled(bool enabled);

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
        uint64_t              parentId = 0;
        glm::vec3             parentOffsetPos {0.0f};
        glm::vec3             parentOffsetRot {0.0f};
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
    RenderMode renderMode = RenderMode::Solid;
    bool centerMesh = true;
    bool preferBin = true;
    static constexpr int kMaxNavmeshSlots = 10;
    int currentNavmeshSlot = 0;
    int currentGeometrySlot = 0;
    std::array<int, kMaxNavmeshSlots> geometrySlotForNavSlot{};
    std::array<std::vector<MeshInstance>, kMaxNavmeshSlots> meshInstancesSlots{};
    std::array<uint64_t, kMaxNavmeshSlots> nextMeshIdSlots{};
    std::array<std::unordered_map<uint64_t, MeshBoundsState>, kMaxNavmeshSlots> meshStateCacheSlots{};
    std::array<int, kMaxNavmeshSlots> pickedMeshIndexSlots{};
    std::array<int, kMaxNavmeshSlots> pickedTriSlots{};
    std::array<NavMeshData, kMaxNavmeshSlots> navmeshDataSlots{};
    std::array<std::vector<glm::vec3>, kMaxNavmeshSlots> navMeshTrisSlots{};
    std::array<std::vector<glm::vec3>, kMaxNavmeshSlots> navMeshLinesSlots{};
    std::array<std::vector<DebugLine>, kMaxNavmeshSlots> navmeshLineBufferSlots{};
    std::array<std::vector<DebugLine>, kMaxNavmeshSlots> offmeshLinkLinesSlots{};
    std::array<std::vector<MemoryHandler::OffmeshLinkSlot>, kMaxNavmeshSlots> memoryOffmeshLinkSlots{};
    std::array<std::vector<DebugLine>, kMaxNavmeshSlots> pathLinesSlots{};
    std::array<dtNavMeshQuery*, kMaxNavmeshSlots> navQuerySlots{};
    std::array<bool, kMaxNavmeshSlots> navQueryReadySlots{};
    std::array<bool, kMaxNavmeshSlots> hasPathStartSlots{};
    std::array<bool, kMaxNavmeshSlots> hasPathTargetSlots{};
    std::array<glm::vec3, kMaxNavmeshSlots> pathStartSlots{};
    std::array<glm::vec3, kMaxNavmeshSlots> pathTargetSlots{};
    std::array<bool, kMaxNavmeshSlots> hasOffmeshStartSlots{};
    std::array<bool, kMaxNavmeshSlots> hasOffmeshTargetSlots{};
    std::array<glm::vec3, kMaxNavmeshSlots> offmeshStartSlots{};
    std::array<glm::vec3, kMaxNavmeshSlots> offmeshTargetSlots{};
    NavmeshGenerationSettings navGenSettings{};
    GtaHandler gtaHandler;
    GtaHandlerMenu gtaHandlerMenu;
    MemoryHandler memoryHandler;
    WebSockets webSockets;
    bool webSocketsEnabled = false;
    std::unordered_map<int, uint64_t> memorySlotToMeshId;

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

    enum class ViewportClickMode
    {
        PickTriangle = 0,
        BuildTileAt = 1,
        RemoveTileAt = 2,
        Pathfind_Normal = 3,
        Pathfind_MinEdge = 4,
        EditMesh = 5,
        AddOffmeshLink = 6,
        RemoveOffmeshLink = 7,
        SetBoundingBox = 8,
        EditBoundingBoxSize = 9
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
    bool offmeshBidirectional = true;
    dtQueryFilter pathQueryFilter{};

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

    // Bounding box editing
    bool boundingBoxVisible = false;
    bool boundingBoxAwaitingSecondClick = false;
    glm::vec3 boundingBoxP0{};
    glm::vec3 boundingBoxP1{};
    glm::vec3 boundingBoxMin{};
    glm::vec3 boundingBoxMax{};
    float boundingBoxAlpha = 0.2f;
    std::vector<DebugLine> boundingBoxLines;
    std::vector<glm::vec3> boundingBoxFill;
    bool boundingBoxDragActive = false;
    GizmoAxis boundingBoxActiveAxis = GizmoAxis::None;
    glm::vec3 boundingBoxGizmoDir{1.0f, 0.0f, 0.0f};
    glm::vec3 boundingBoxStartMin{};
    glm::vec3 boundingBoxStartMax{};
    float boundingBoxStartParam = 0.0f;
    int boundingBoxDragSign = 1;

    void ClearBoundingBox();
    void RebuildBoundingBoxDebug();
    void DrawBoundingBoxGizmo();
    bool BeginBoundingBoxDrag(const Ray& ray);
    void UpdateBoundingBoxDrag(int mouseX, int mouseY);

private:
    bool InitSDL();
    bool InitGL();
    bool InitImGui();

    bool LoadMeshFromPath(const std::string& path);
    bool LoadMeshFromPathWithOptions(const std::string& path, bool center, bool tryLoadBin);
    void LoadLastDirectory();
    void SaveLastDirectory(const std::filesystem::path& directory);
    std::filesystem::path GetConfigFilePath() const;
    std::filesystem::path ResolveMemoryHandlerObjPath(const std::string& propName) const;

    void RemoveMesh(size_t index);
    void ClearMeshes();
    void HandleAutoBuild(NavmeshAutoBuildFlag flag);
    glm::mat4 GetModelMatrix(const MeshInstance& instance) const;
    MeshBoundsState ComputeMeshBounds(const MeshInstance& instance) const;
    bool HasMeshChanged(const MeshBoundsState& previous, const MeshBoundsState& current) const;
    void UpdateNavmeshTiles();
    bool ApplyAutomaticOffmeshLinks(const AutoOffmeshGenerationParams& params);
    bool ComputeRayMeshHit(int mx, int my, glm::vec3& outPoint, int* outTri, int* outMeshIndex);
    void ResetPathState(int slotIndex = -1);
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
    MeshInstance* FindMeshById(uint64_t id);
    const MeshInstance* FindMeshById(uint64_t id) const;
    void UpdateChildTransforms(uint64_t parentId);
    void OnInstanceTransformUpdated(size_t index, bool positionChanged, bool rotationChanged);
    bool WouldCreateCycle(uint64_t childId, uint64_t newParentId) const;
    void SetMeshParent(size_t childIndex, uint64_t newParentId);
    void DetachChildren(uint64_t parentId);
    void CollectSubtreeIds(uint64_t rootId, std::vector<uint64_t>& outIds) const;
    void RemoveMeshSubtree(uint64_t rootId);
    bool IsMeshAlreadyLoaded(const std::filesystem::path& path) const;
    void ProcessMemoryGeometryRequests();
    void ProcessMemoryRouteRequests();
    void ProcessMemoryOffmeshLinks();
    void ProcessMemoryBoundingBox();
    glm::vec3 FromGtaRotation(const glm::vec3& gtaRot) const;
    int FindMeshIndexById(uint64_t id) const;

    void ProcessEvents();
    void RenderFrame();

    void ProcessNavmeshJob();
    void JoinNavmeshWorker();
    void RequestStopNavmeshBuild();
    void ClearNavmesh();
    void RebuildDebugLineBuffer(int slotIndex = -1);
    void RebuildOffmeshLinkLines(int slotIndex = -1);
    bool IsNavmeshJobRunning() const { return navmeshJobRunning.load(); }

    ImGui::FileBrowser directoryBrowser;
    ImGui::FileBrowser propHashBrowser;
    ImGui::FileBrowser objDirectoryBrowser;
    std::string currentDirectory;
    std::string selectedEntry;

    bool showSidePanel = true;
    bool showMeshBrowserWindow = true;
    bool showDebugInfo = true;
    char meshFilter[64] = {0};

    bool rightButtonDown = false;
    bool rightButtonDragged = false;
    int rightButtonDownX = 0;
    int rightButtonDownY = 0;
    bool drawMeshOutlines = true;

    struct NavmeshJobResult
    {
        bool success = false;
        int slotIndex = 0;
        NavMeshData navData;
        std::vector<glm::vec3> tris;
        std::vector<glm::vec3> lines;
    };

    std::thread navmeshWorker;
    std::atomic<bool> navmeshJobRunning{false};
    std::atomic<bool> navmeshJobCompleted{false};
    std::atomic<bool> navmeshCancelRequested{false};
    bool navmeshJobQueued = false;
    bool navmeshJobQueuedBuildTilesNow = true;
    int navmeshJobQueuedSlot = -1;
    std::mutex navmeshJobMutex;
    std::unique_ptr<NavmeshJobResult> navmeshJobResult;

    // Procedural loading helpers
    void RunProceduralTestStep();
    bool PrepareProceduralNavmeshIfNeeded();
    void BuildProceduralTilesAroundCamera();

    NavMeshData& CurrentNavData() { return navmeshDataSlots[currentNavmeshSlot]; }
    const NavMeshData& CurrentNavData() const { return navmeshDataSlots[currentNavmeshSlot]; }
    std::vector<MeshInstance>& CurrentMeshes() { return meshInstancesSlots[currentGeometrySlot]; }
    const std::vector<MeshInstance>& CurrentMeshes() const { return meshInstancesSlots[currentGeometrySlot]; }
    std::unordered_map<uint64_t, MeshBoundsState>& CurrentMeshStateCache() { return meshStateCacheSlots[currentGeometrySlot]; }
    const std::unordered_map<uint64_t, MeshBoundsState>& CurrentMeshStateCache() const { return meshStateCacheSlots[currentGeometrySlot]; }
    uint64_t& CurrentNextMeshId() { return nextMeshIdSlots[currentGeometrySlot]; }
    int& CurrentPickedMeshIndex() { return pickedMeshIndexSlots[currentGeometrySlot]; }
    int& CurrentPickedTri() { return pickedTriSlots[currentGeometrySlot]; }
    std::vector<glm::vec3>& CurrentNavMeshTris() { return navMeshTrisSlots[currentNavmeshSlot]; }
    std::vector<glm::vec3>& CurrentNavMeshLines() { return navMeshLinesSlots[currentNavmeshSlot]; }
    std::vector<DebugLine>& CurrentNavmeshLineBuffer() { return navmeshLineBufferSlots[currentNavmeshSlot]; }
    std::vector<DebugLine>& CurrentOffmeshLinkLines() { return offmeshLinkLinesSlots[currentNavmeshSlot]; }
    std::vector<DebugLine>& CurrentPathLines() { return pathLinesSlots[currentNavmeshSlot]; }
    dtNavMeshQuery*& CurrentNavQuery() { return navQuerySlots[currentNavmeshSlot]; }
    bool& CurrentNavQueryReady() { return navQueryReadySlots[currentNavmeshSlot]; }
    bool& CurrentHasPathStart() { return hasPathStartSlots[currentNavmeshSlot]; }
    bool& CurrentHasPathTarget() { return hasPathTargetSlots[currentNavmeshSlot]; }
    glm::vec3& CurrentPathStart() { return pathStartSlots[currentNavmeshSlot]; }
    glm::vec3& CurrentPathTarget() { return pathTargetSlots[currentNavmeshSlot]; }
    bool& CurrentHasOffmeshStart() { return hasOffmeshStartSlots[currentNavmeshSlot]; }
    bool& CurrentHasOffmeshTarget() { return hasOffmeshTargetSlots[currentNavmeshSlot]; }
    glm::vec3& CurrentOffmeshStart() { return offmeshStartSlots[currentNavmeshSlot]; }
    glm::vec3& CurrentOffmeshTarget() { return offmeshTargetSlots[currentNavmeshSlot]; }
    void SwitchNavmeshSlot(int slotIndex);
    bool IsValidSlot(int slotIndex) const { return slotIndex >= 0 && slotIndex < kMaxNavmeshSlots; }
    void ClearNavmeshSlotData(int slotIndex);
    void ResetAllNavmeshSlots();

    bool proceduralTestEnabled = false;
    bool proceduralHasLastScan = false;
    glm::vec3 proceduralLastScanGtaPos{0.0f};
    bool proceduralNavmeshPending = false;
    bool proceduralNavmeshNeedsRebuild = false;
    std::unordered_set<uint64_t> proceduralBuiltTiles;
};