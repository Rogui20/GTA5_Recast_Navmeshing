#pragma once

#include "GtaNavContext.h"

#ifdef _WIN32
  #define GTANAV_API extern "C" __declspec(dllexport)
#else
  #define GTANAV_API extern "C"
#endif

// Cria/destroi contexto
GTANAV_API NavMeshContext* GtaNav_InitNavMesh();
GTANAV_API void            GtaNav_FreeNavMesh(NavMeshContext* ctx);

// Define parâmetros (equivalente ao seu antigo SetNavMeshDefinition)
GTANAV_API void GtaNav_DefineNavMesh(NavMeshContext* ctx,
                                     const NavMeshDefinition* def,
                                     int navmeshType,
                                     int mapID);

// Inicializa contexto dinâmico (AABB, tile grid)
GTANAV_API bool GtaNav_InitDynamicContext(NavMeshContext* ctx,
                                          const float* bmin,
                                          const float* bmax);

// Consulta se está pronto
GTANAV_API bool GtaNav_IsDynamicContextReady(NavMeshContext* ctx);

// Geometria (por enquanto só um stub pra você ligar no seu pipeline atual)
GTANAV_API bool GtaNav_SetCombinedGeometry(NavMeshContext* ctx,
                                           const float* verts,
                                           int nverts,
                                           const int* tris,
                                           int ntris);

// Tiles
GTANAV_API bool GtaNav_BuildTilesAroundPositionAPI(NavMeshContext* ctx,
                                                   const float* pos,
                                                   int numTilesX,
                                                   int numTilesZ);

// Cache
GTANAV_API bool GtaNav_SaveTileCache(NavMeshContext* ctx, const char* cachePath);
GTANAV_API bool GtaNav_LoadTileCache(NavMeshContext* ctx,
                                     const char* cachePath,
                                     bool validateGeometry,
                                     bool rewriteOutdatedCache);
