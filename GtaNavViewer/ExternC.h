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

// Navmesh lifecycle
GTANAVVIEWER_API bool BuildNavMesh(void* navMesh);
GTANAVVIEWER_API bool UpdateNavMesh(void* navMesh);

// Streaming / bake
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
GTANAVVIEWER_API bool GenerateAutomaticOffmeshLinks(void* navMesh);

// Bounding box de build
GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax);
GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh);
