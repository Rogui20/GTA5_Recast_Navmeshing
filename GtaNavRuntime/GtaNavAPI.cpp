#include "GtaNavAPI.h"
#include "InputGeom.h"
#include "GtaNavTiles.h"

GTANAV_API NavMeshContext* GtaNav_InitNavMesh()
{
    return GtaNav_CreateContext();
}

GTANAV_API void GtaNav_FreeNavMesh(NavMeshContext* ctx)
{
    GtaNav_DestroyContext(ctx);
}

GTANAV_API void GtaNav_DefineNavMesh(NavMeshContext* ctx,
                                     const NavMeshDefinition* def,
                                     int navmeshType,
                                     int mapID)
{
    if (!ctx || !def) return;
    GtaNav_SetNavMeshDefinition(ctx, *def, navmeshType, mapID);
}

GTANAV_API bool GtaNav_InitDynamicContext(NavMeshContext* ctx,
                                          const float* bmin,
                                          const float* bmax)
{
    return GtaNav_InitDynamicNavMesh(ctx, bmin, bmax);
}

GTANAV_API bool GtaNav_IsDynamicContextReady(NavMeshContext* ctx)
{
    if (!ctx) return false;
    return ctx->navMesh != nullptr && ctx->navQuery != nullptr;
}

GTANAV_API bool GtaNav_SetCombinedGeometry(NavMeshContext* ctx,
                                           const float* verts,
                                           int nverts,
                                           const int* tris,
                                           int ntris)
{
    if (!ctx || !verts || !tris || nverts <= 0 || ntris <= 0)
        return false;

    if (!ctx->geom)
        ctx->geom = new InputGeom();

    if (!ctx->geom->initMeshFromArrays(&ctx->buildCtx, verts, nverts, tris, ntris))
    {
        printf("[GtaNav] GtaNav_SetCombinedGeometry: initMeshFromArrays falhou.\n");
        return false;
    }

    return true;
}

GTANAV_API bool GtaNav_BuildTilesAroundPositionAPI(NavMeshContext* ctx,
                                                   const float* pos,
                                                   int numTilesX,
                                                   int numTilesZ)
{
    if (!ctx || !ctx->navMesh || !ctx->geom)
        return false;

    // Converte "quantidade de tiles" para metros
    // (usamos o tamanho de tile definido na navDef)
    float tileSize = ctx->navDef.navDef_m_tileSize;

    // Usa X e Z — mas pegamos o maior para cobrir um retângulo como um círculo
    float radiusMeters = (float)(std::max(numTilesX, numTilesZ)) * tileSize;

    return GtaNavTiles::BuildTilesAroundPosition(ctx, pos, radiusMeters);
}
