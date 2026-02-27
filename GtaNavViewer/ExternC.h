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

// Bounding box de build
GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax);
GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh);
