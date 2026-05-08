#include "GtaNavContext.h"
#include <cstdio>
#include <cmath>
#include "DetourCommon.h"

NavMeshContext* GtaNav_CreateContext()
{
    NavMeshContext* ctx = new NavMeshContext();
    if (!ctx)
        return nullptr;

    // Bounds zerados por padrão
    dtVset(ctx->dynamicBMin, 0, 0, 0);
    dtVset(ctx->dynamicBMax, 0, 0, 0);
    dtVset(ctx->staticBMin, 0, 0, 0);
    dtVset(ctx->staticBMax, 0, 0, 0);

    return ctx;
}

void GtaNav_DestroyContext(NavMeshContext* ctx)
{
    if (!ctx) return;

    if (ctx->navQuery)
    {
        dtFreeNavMeshQuery(ctx->navQuery);
        ctx->navQuery = nullptr;
    }

    if (ctx->navMesh)
    {
        dtFreeNavMesh(ctx->navMesh);
        ctx->navMesh = nullptr;
    }

    if (ctx->geom)
    {
        delete ctx->geom;
        ctx->geom = nullptr;
    }

    delete ctx;
}

void GtaNav_SetNavMeshDefinition(NavMeshContext* ctx, const NavMeshDefinition& def, int navmeshType, int mapID)
{
    if (!ctx) return;
    ctx->navDef = def;
    ctx->navmeshType = navmeshType;
    ctx->mapID = mapID;
}

// Helpers internos para bits
static int ilog2i(int v)
{
    int r = 0;
    while ((v >>= 1) != 0) r++;
    return r;
}

static int nextPow2i(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bool GtaNav_InitDynamicNavMesh(NavMeshContext* ctx, const float* bmin, const float* bmax)
{
    if (!ctx || !bmin || !bmax)
        return false;

    if (bmax[0] <= bmin[0] || bmax[2] <= bmin[2])
    {
        printf("[GtaNav] InitDynamicNavMesh: AABB invalido.\n");
        return false;
    }

    // Libera navmesh/query antigos se houver
    if (ctx->navQuery)
    {
        dtFreeNavMeshQuery(ctx->navQuery);
        ctx->navQuery = nullptr;
    }
    if (ctx->navMesh)
    {
        dtFreeNavMesh(ctx->navMesh);
        ctx->navMesh = nullptr;
    }

    ctx->navMesh = dtAllocNavMesh();
    if (!ctx->navMesh)
    {
        printf("[GtaNav] Falha ao alocar dtNavMesh.\n");
        return false;
    }

    const float cellSize   = ctx->navDef.navDef_m_cellSize;
    const float tileCells  = ctx->navDef.navDef_m_tileSize;  // ex: 64
    const float tileWorld  = tileCells * cellSize;

    const float width  = bmax[0] - bmin[0];
    const float depth  = bmax[2] - bmin[2];

    const int gw = rcMax(1, (int)ceilf(width  / tileWorld));
    const int gh = rcMax(1, (int)ceilf(depth / tileWorld));

    const int totalTiles = gw * gh;
    int tileBits = rcMin(ilog2i(nextPow2i(totalTiles)), 14);
    int polyBits = 22 - tileBits;

    dtNavMeshParams params{};
    // Origem do grid – você pode usar bmin pra alinhar com o AABB
    params.orig[0] = bmin[0];
    params.orig[1] = bmin[1];
    params.orig[2] = bmin[2];

    params.tileWidth  = tileWorld;
    params.tileHeight = tileWorld;

    params.maxTiles = 1 << tileBits;
    params.maxPolys = 1 << polyBits;

    if (dtStatusFailed(ctx->navMesh->init(&params)))
    {
        printf("[GtaNav] Falha ao inicializar dtNavMesh.\n");
        dtFreeNavMesh(ctx->navMesh);
        ctx->navMesh = nullptr;
        return false;
    }

    ctx->navQuery = new dtNavMeshQuery();
    if (!ctx->navQuery || dtStatusFailed(ctx->navQuery->init(ctx->navMesh, 2048)))
    {
        printf("[GtaNav] Falha ao inicializar dtNavMeshQuery.\n");
        if (ctx->navQuery) delete ctx->navQuery;
        ctx->navQuery = nullptr;
        dtFreeNavMesh(ctx->navMesh);
        ctx->navMesh = nullptr;
        return false;
    }

    ctx->navmeshGenerated = true;

    printf("[GtaNav] InitDynamicNavMesh OK. grid=%dx%d maxTiles=%d tileWorld=%.2f\n",
           gw, gh, params.maxTiles, tileWorld);

    return true;
}
