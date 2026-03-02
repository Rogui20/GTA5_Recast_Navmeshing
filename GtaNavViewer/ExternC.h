#pragma once

#include <cstddef>
#include <cstdint>

#include "NavMeshData.h"

struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct NavMeshEdgeInfo
{
    Vector3 vertexA;
    Vector3 vertexB;
    Vector3 center;
    Vector3 normal;
    std::uint64_t polygonId = 0;
};

struct NavMeshPolygonInfo
{
    std::uint64_t polygonId = 0;
    Vector3 center;
    Vector3 normal;
    int vertexStart = 0;
    int vertexCount = 0;
    int edgeStart = 0;
    int edgeCount = 0;
};

struct NavMeshGeometryInfo
{
    char customID[128]{};
    Vector3 position;
    Vector3 rotation;
    int vertexCount = 0;
    int indexCount = 0;
};

struct OffMeshLinkInfo
{
    Vector3 start;
    Vector3 end;
    float radius = 0.0f;
    bool biDir = true;
    unsigned char area = 0;
    unsigned short flags = 0;
    unsigned int userId = 0;
    int ownerTx = -1;
    int ownerTy = -1;
};

enum SimShapeType : std::uint8_t
{
    SHAPE_CYLINDER = 0,
    SHAPE_BOX = 1,
};

enum DynObsShapeType : std::uint8_t
{
    DYNOBS_CYLINDER = 0,
    DYNOBS_BOX_AABB = 1,
};

struct DynObstacleDescFFI
{
    std::uint32_t obstacleId = 0;
    std::uint32_t teamMask = 0;
    std::uint32_t avoidMask = 0;

    std::uint8_t shapeType = 0;
    std::uint8_t _pad[3]{};

    float pos[3]{};
    float radius = 0.0f;
    float halfX = 0.0f;
    float halfZ = 0.0f;
    float height = 0.0f;
};

struct PathAvoidParamsFFI
{
    float inflate = 0.0f;
    float detourSideStep = 2.0f;
    int maxDetourCandidates = 6;
    int maxObstaclesToCheck = 64;
    int maxFixIterations = 8;
    std::uint8_t useHeightFilter = 1;
    float heightTolerance = 2.5f;
    std::uint8_t _pad[3]{};
};

enum SimAgentFlags : std::uint32_t
{
    AGENT_ENABLED = 1u << 0,
    AGENT_VEHICLE = 1u << 1,
    // Reset forte do estado (teleport).
    AGENT_TELEPORT = 1u << 2,
    // Reancora estado no input sem destruir steering por completo.
    AGENT_ANCHOR = 1u << 3,
    // Com AGENT_ANCHOR: corrige heading com blend/snap.
    AGENT_ANCHOR_HEADING = 1u << 4,
    // Com AGENT_ANCHOR: corrige velocidade com blend/snap.
    AGENT_ANCHOR_VELOCITY = 1u << 5,
};

enum SimPathModeFlags : std::uint32_t
{
    // Modo padrão: recalcula path preservando estado dinâmico do agente.
    SIM_PATH_SOFT_REPATH = 0u,
    // Força reset completo de estado/corredor e reposiciona no nearest do start informado.
    SIM_PATH_HARD_RESET = 1u << 0,
    // No soft repath: tenta manter cornerIndex atual quando ainda é válido.
    SIM_PATH_KEEP_CORNER_INDEX_IF_VALID = 1u << 1,
    // No soft repath: escolhe corner pelo segmento mais próximo em vez de vértice mais próximo.
    SIM_PATH_FIND_CORNER_BY_SEGMENT = 1u << 2,
};

struct SimAgentDescFFI
{
    std::uint32_t agentId = 0;
    std::uint32_t teamMask = 0;
    std::uint32_t avoidMask = 0;
    std::uint32_t flags = 0;
    std::uint8_t shapeType = 0;
    std::uint8_t _pad[3]{};

    float pos[3]{};
    // Velocidade opcional para reanchor/teleport.
    float vel[3]{};
    float headingDeg = 0.0f;

    float radius = 0.0f;
    float halfX = 0.0f;
    float halfZ = 0.0f;
    float height = 0.0f;
};

struct SimParamsFFI
{
    float agentSpeed = 0.0f;
    float agentAccel = 0.0f;
    float agentTurnSpeedDeg = 0.0f;
    float lookAheadDist = 0.0f;
    float reachRadius = 0.0f;
    float avoidWeight = 0.0f;
    float avoidRange = 0.0f;
    float wallAvoidWeight = 0.0f;
    float wallAvoidDist = 0.0f;
    float gravity = 0.0f;
    float maxFallSpeed = 0.0f;
    float maxSpeedForward = 0.0f;
    float maxSpeedReverse = 0.0f;
    float brakeDecel = 0.0f;

    // Blend de heading durante AGENT_ANCHOR + AGENT_ANCHOR_HEADING.
    float anchorHeadingAlpha = 0.25f;
    // Blend de velocidade durante AGENT_ANCHOR + AGENT_ANCHOR_VELOCITY.
    float anchorVelAlpha = 0.35f;
    // Se distancia de anchor exceder esse valor, aplica snap forte (estilo teleport).
    float anchorMaxSnapDist = 15.0f;
    // Se delta de heading exceder esse valor, aplica snap de yaw ao inves de blend.
    float anchorMaxSnapYawDeg = 90.0f;

    // Vehicle ground fit / suspension
    float vehicleGroundUpOffset = 0.2f;
    float vehicleSuspensionAlpha = 0.35f;
    float vehicleRotAlpha = 0.35f;
    float vehicleMaxPitchDeg = 25.0f;
    float vehicleMaxRollDeg = 25.0f;
    float vehicleSampleUp = 3.0f;
    float vehicleSampleDown = 10.0f;
    float vehicleWheelInset = 0.2f;
};

struct SimEventFFI
{
    std::uint32_t agentId = 0;
    std::uint16_t frameIndex = 0;
    std::uint8_t type = 0;
    std::uint8_t jumpType = 0;
    float start[3]{};
    float end[3]{};
    float duration = 0.0f;
};

static_assert(sizeof(SimAgentDescFFI) == 64, "Unexpected SimAgentDescFFI ABI size");
static_assert(sizeof(SimParamsFFI) == 104, "Unexpected SimParamsFFI ABI size");
static_assert(sizeof(DynObstacleDescFFI) == 44, "Unexpected DynObstacleDescFFI ABI size");
static_assert(sizeof(PathAvoidParamsFFI) == 32, "Unexpected PathAvoidParamsFFI ABI size");

#ifdef _WIN32
  #ifdef GTANAVVIEWER_BUILD_DLL
    #define GTANAVVIEWER_API extern "C" __declspec(dllexport)
  #else
    #define GTANAVVIEWER_API extern "C" __declspec(dllimport)
  #endif
#else
  #define GTANAVVIEWER_API extern "C"
#endif

// Criação / configuração
GTANAVVIEWER_API void* InitNavMesh();
GTANAVVIEWER_API void  DestroyNavMeshResources(void* navMesh);
GTANAVVIEWER_API void  SetNavMeshGenSettings(void* navMesh, const NavmeshGenerationSettings* settings);
GTANAVVIEWER_API void  SetAutoOffMeshGenerationParams(void* navMesh, const AutoOffmeshGenerationParams* params);
GTANAVVIEWER_API void  SetNavMeshCacheRoot(void* navMesh, const char* cacheRoot);
GTANAVVIEWER_API void  SetNavMeshSessionId(void* navMesh, const char* sessionId);
GTANAVVIEWER_API void  SetMaxResidentTiles(void* navMesh, int maxTiles);
GTANAVVIEWER_API bool  SaveNavMeshRuntimeCache(void* navMesh, const char* cacheFilePath);
GTANAVVIEWER_API bool  LoadNavMeshRuntimeCache(void* navMesh, const char* cacheFilePath);

// Geometria dinâmica
GTANAVVIEWER_API bool AddGeometry(void* navMesh,
                                  const char* pathToGeometry,
                                  Vector3 pos,
                                  Vector3 rot,
                                  const char* customID,
                                  bool preferBIN);
GTANAVVIEWER_API bool UpdateGeometry(void* navMesh,
                                     const char* customID,
                                     const Vector3* pos,
                                     const Vector3* rot,
                                     bool updateNavMesh);
GTANAVVIEWER_API bool RemoveGeometry(void* navMesh, const char* customID);
GTANAVVIEWER_API void RemoveAllGeometries(void* navMesh);
GTANAVVIEWER_API int  GetAllGeometries(void* navMesh,
                                       NavMeshGeometryInfo* geometries,
                                       int maxGeometries,
                                       int* outGeometryCount);
GTANAVVIEWER_API bool ExportMergedGeometriesObj(void* navMesh, const char* outputObjPath);

// Navmesh lifecycle
GTANAVVIEWER_API bool BuildNavMesh(void* navMesh);
GTANAVVIEWER_API bool UpdateNavMesh(void* navMesh);

// Streaming / bake
GTANAVVIEWER_API bool InitTiledGrid(void* navMesh, Vector3 bmin, Vector3 bmax);
GTANAVVIEWER_API int  StreamTilesAround(void* navMesh, Vector3 center, float radius, bool allowBuildIfMissing);
GTANAVVIEWER_API void ClearAllLoadedTiles(void* navMesh);
GTANAVVIEWER_API bool BakeTilesInBounds(void* navMesh, Vector3 bmin, Vector3 bmax, bool saveToCache);

// Pathfind
GTANAVVIEWER_API int FindPath(void* navMesh,
                              Vector3 start,
                              Vector3 target,
                              int flags,
                              int maxPoints,
                              float* outPath, int options);
GTANAVVIEWER_API int FindPathWithMinEdge(void* navMesh,
                                         Vector3 start,
                                         Vector3 target,
                                         int flags,
                                         int maxPoints,
                                         float minEdge,
                                         float* outPath, int options);
GTANAVVIEWER_API int FindPathWithNodeMetadata(void* navMesh,
                                              Vector3 start,
                                              Vector3 target,
                                              int flags,
                                              int maxPoints,
                                              float minEdgeDist,
                                              float* outPath,
                                              bool* outIsOffMeshLinkNode,
                                              bool* outIsNextOffMeshLinkNodeHigher,
                                              int options);
GTANAVVIEWER_API int FindPathAvoidingDynamicObstacles(void* navMesh,
                                                      Vector3 start,
                                                      Vector3 target,
                                                      int flags,
                                                      int maxPoints,
                                                      float minEdgeDist,
                                                      int options,
                                                      const PathAvoidParamsFFI* avoidParams,
                                                      std::uint32_t selfTeamMask,
                                                      std::uint32_t selfAvoidMask,
                                                      float* outPathXYZ,
                                                      std::uint8_t* outUsedDetour);

// Consulta da malha
GTANAVVIEWER_API int GetNavMeshPolygons(void* navMesh,
                                        NavMeshPolygonInfo* polygons,
                                        int maxPolygons,
                                        Vector3* vertices,
                                        int maxVertices,
                                        NavMeshEdgeInfo* edges,
                                        int maxEdges,
                                        int* outVertexCount,
                                        int* outEdgeCount);
GTANAVVIEWER_API int GetNavMeshBorderEdges(void* navMesh,
                                           NavMeshEdgeInfo* edges,
                                           int maxEdges,
                                           int* outEdgeCount);

// Offmesh links
GTANAVVIEWER_API bool AddOffMeshLink(void* navMesh,
                                     Vector3 start,
                                     Vector3 end,
                                     bool biDir,
                                     bool updateNavMesh);
GTANAVVIEWER_API int AddOffmeshLinksToNavMeshIsland(void* navMesh,
                                                    const IslandOffmeshLinkParams* params,
                                                    OffmeshLink* outLinks,
                                                    int maxLinks);
GTANAVVIEWER_API void ClearOffMeshLinks(void* navMesh, bool updateNavMesh);
GTANAVVIEWER_API int GetOffMeshLinks(void* navMesh,
                                     OffMeshLinkInfo* outLinks,
                                     int maxLinks,
                                     int* outLinkCount);
GTANAVVIEWER_API int RemoveOffMeshLinksInRadius(void* navMesh,
                                                Vector3 center,
                                                float radius,
                                                bool updateNavMesh);
GTANAVVIEWER_API bool RemoveOffMeshLinkById(void* navMesh,
                                            unsigned int userId,
                                            bool updateNavMesh);
GTANAVVIEWER_API bool GenerateAutomaticOffmeshLinks(void* navMesh);

// Simulação de agentes
GTANAVVIEWER_API int UpsertSimAgents(void* navMesh, const SimAgentDescFFI* agents, int count);
GTANAVVIEWER_API int RemoveSimAgents(void* navMesh, const std::uint32_t* agentIds, int count);
GTANAVVIEWER_API void ClearSimAgents(void* navMesh);
GTANAVVIEWER_API int UpsertDynamicObstacles(void* navMesh, const DynObstacleDescFFI* obs, int count);
GTANAVVIEWER_API int RemoveDynamicObstacles(void* navMesh, const std::uint32_t* obstacleIds, int count);
GTANAVVIEWER_API void ClearDynamicObstacles(void* navMesh);
GTANAVVIEWER_API int ComputeAgentPath(void* navMesh,
                                      std::uint32_t agentId,
                                      Vector3 start,
                                      Vector3 target,
                                      int flags,
                                      int maxCorners,
                                      float minEdgeDist,
                                      int options);
GTANAVVIEWER_API int ComputeAgentPathEx(void* navMesh,
                                        std::uint32_t agentId,
                                        Vector3 start,
                                        Vector3 target,
                                        int flags,
                                        int maxCorners,
                                        float minEdgeDist,
                                        int options,
                                        std::uint32_t pathModeFlags);
GTANAVVIEWER_API void EnableHeightSampling(void* navMesh, bool enabled);
GTANAVVIEWER_API bool BuildHeightSamplerForCurrentGeometry(void* navMesh, int samplesPerTile, bool storeTwoLayers);
GTANAVVIEWER_API int SimulateAgentFrames(void* navMesh,
                                         std::uint32_t agentId,
                                         float dt,
                                         int maxSimulationFrames,
                                         const SimParamsFFI* params,
                                         float* outPosXYZ,
                                         float* outHeadingDeg,
                                         float* outVelXYZ,
                                         std::uint8_t* outFrameFlags,
                                         float* outEulerRPYDeg,
                                         SimEventFFI* outEvents,
                                         int maxEvents);
GTANAVVIEWER_API int SimulateAgentsFramesBatch(void* navMesh,
                                               const std::uint32_t* agentIds,
                                               int agentCount,
                                               float dt,
                                               int maxSimulationFrames,
                                               const SimParamsFFI* params,
                                               float* outPosXYZ,
                                               float* outHeadingDeg,
                                               float* outVelXYZ,
                                               std::uint8_t* outFrameFlags,
                                               float* outEulerRPYDeg,
                                               SimEventFFI* outEvents,
                                               int maxEvents);

// Bounding box de build
GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax);
GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh);
