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
GTANAVVIEWER_API void  SetNavMeshGenSettings(void* navMesh, const NavmeshGenerationSettings* settings);

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

// Pathfind
GTANAVVIEWER_API int FindPath(void* navMesh,
                              Vector3 start,
                              Vector3 target,
                              int flags,
                              int maxPoints,
                              float* outPath);
GTANAVVIEWER_API int FindPathWithMinEdge(void* navMesh,
                                         Vector3 start,
                                         Vector3 target,
                                         int flags,
                                         int maxPoints,
                                         float minEdge,
                                         float* outPath);

// Offmesh links
GTANAVVIEWER_API bool AddOffMeshLink(void* navMesh,
                                     Vector3 start,
                                     Vector3 end,
                                     bool biDir,
                                     bool updateNavMesh);
GTANAVVIEWER_API void ClearOffMeshLinks(void* navMesh, bool updateNavMesh);

// Bounding box de build
GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax);
GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh);

