// ======================================================================
// GtaNavTiles.cpp (versão revisada e otimizada)
// ======================================================================

#include <vector>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "GtaNavTiles.h"
#include "GtaNavContext.h"
#include "GtaNavGeometry.h"
#include "InputGeom.h"

#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include "DetourNavMeshBuilder.h"


// =======================================================================
// CALCULATE TILE BOUNDS
// =======================================================================
void GtaNavTiles::CalcTileBounds(
    const NavMeshContext* ctx,
    int tx, int tz,
    float bmin[3], float bmax[3])
{
    const dtNavMeshParams* params = ctx->navMesh->getParams();
    const float* orig = params->orig;

    const float tileW = params->tileWidth;
    const float tileH = params->tileHeight;

    // Bounds XZ
    bmin[0] = orig[0] + tx * tileW;
    bmin[2] = orig[2] + tz * tileH;

    bmax[0] = orig[0] + (tx + 1) * tileW;
    bmax[2] = orig[2] + (tz + 1) * tileH;

    // Bounds Y = full scene AABB
    const float* gm = ctx->geom->getMeshBoundsMin();
    const float* gM = ctx->geom->getMeshBoundsMax();

    bmin[1] = gm[1];
    bmax[1] = gM[1];
}


// =======================================================================
// TILE RANGE FROM WORLD AABB
// =======================================================================
bool GtaNavTiles::CalcTileRangeForAABB(
    const NavMeshContext* ctx,
    const float bmin[3],
    const float bmax[3],
    int& tx0, int& tz0, int& tx1, int& tz1)
{
    if (!ctx || !ctx->navMesh)
        return false;

    const dtNavMeshParams* params = ctx->navMesh->getParams();
    const float* orig = params->orig;
    const float tileW = params->tileWidth;
    const float tileH = params->tileHeight;

    tx0 = (int)floorf((bmin[0] - orig[0]) / tileW);
    tz0 = (int)floorf((bmin[2] - orig[2]) / tileH);
    tx1 = (int)floorf((bmax[0] - orig[0]) / tileW);
    tz1 = (int)floorf((bmax[2] - orig[2]) / tileH);

    if (tx1 < tx0 || tz1 < tz0)
        return false;

    return true;
}


// =======================================================================
// BUILD 1 TILE
// =======================================================================
bool GtaNavTiles::BuildTile(
    NavMeshContext* ctx,
    int tx, int tz,
    const float tileBMin[3],
    const float tileBMax[3],
    bool rebuildDetourData)
{
    if (!ctx || !ctx->navMesh || !ctx->geom)
    {
        printf("[Tiles] BuildTile: contexto/navMesh/geom nulos.\n");
        return false;
    }

    // Remove tile antigo se existir
    if (dtTileRef oldRef = ctx->navMesh->getTileRefAt(tx, tz, 0))
        ctx->navMesh->removeTile(oldRef, nullptr, nullptr);

    // 1) Obter geometria recortada
    std::vector<float> verts;
    std::vector<int>   tris;

    if (!GtaNavGeometry::BuildTileGeometry(
            ctx,
            tileBMin,
            tileBMax,
            verts,
            tris))
    {
        // Sem geometria → tile vazio (ok)
        return true;
    }

    int nverts = (int)verts.size() / 3;
    int ntris  = (int)tris.size() / 3;

    if (nverts == 0 || ntris == 0)
        return true;

    // =====================================================================
    // 2) CONFIG RECAST
    // =====================================================================
    const NavMeshDefinition& def = ctx->navDef;
    const float cs = def.navDef_m_cellSize;
    const float ch = def.navDef_m_cellHeight;

    const dtNavMeshParams* navParams = ctx->navMesh->getParams();

    rcConfig cfg{};
    memset(&cfg, 0, sizeof(cfg));

    cfg.cs = cs;
    cfg.ch = ch;
    cfg.walkableSlopeAngle = def.navDef_m_agentMaxSlope;
    cfg.walkableHeight     = (int)ceilf(def.navDef_m_agentHeight / ch);
    cfg.walkableClimb      = (int)floorf(def.navDef_m_agentMaxClimb / ch);
    cfg.walkableRadius     = (int)ceilf(def.navDef_m_agentRadius / cs);

    // ⚠ Atenção: def.navDef_m_edgeMaxLen DEVE estar em metros.
    cfg.maxEdgeLen = (int)(def.navDef_m_edgeMaxLen / cs);

    cfg.maxSimplificationError = def.navDef_m_edgeMaxError;
    cfg.minRegionArea      = (int)rcSqr(def.navDef_m_regionMinSize);
    cfg.mergeRegionArea    = (int)rcSqr(def.navDef_m_regionMergeSize);
    cfg.maxVertsPerPoly    = (int)def.navDef_m_vertsPerPoly;

    // ✔ comportamento estável
    cfg.detailSampleDist   = def.navDef_m_detailSampleDist;
    cfg.detailSampleMaxError = ch * def.navDef_m_detailSampleMaxError;

    // Tile size in cells
    cfg.width  = (int)(navParams->tileWidth  / cs);
    cfg.height = (int)(navParams->tileHeight / cs);

    rcVcopy(cfg.bmin, tileBMin);
    rcVcopy(cfg.bmax, tileBMax);

    // =====================================================================
    // 3) FULL RECAST PIPELINE
    // =====================================================================
    rcContext& bc = ctx->buildCtx;

    // Heightfield
    rcHeightfield* solid = rcAllocHeightfield();
    if (!solid)
        return false;

    if (!rcCreateHeightfield(&bc, *solid,
                             cfg.width, cfg.height,
                             cfg.bmin, cfg.bmax,
                             cfg.cs, cfg.ch))
    {
        rcFreeHeightField(solid);
        return false;
    }

    std::vector<unsigned char> areas(ntris, RC_WALKABLE_AREA);

    if (!rcRasterizeTriangles(&bc,
                              verts.data(), nverts,
                              tris.data(), areas.data(),
                              ntris, *solid,
                              cfg.walkableClimb))
    {
        rcFreeHeightField(solid);
        return false;
    }

    rcFilterLowHangingWalkableObstacles(&bc, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&bc, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&bc, cfg.walkableHeight, *solid);

    // CompactHeightfield
    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    if (!chf)
    {
        rcFreeHeightField(solid);
        return false;
    }

    if (!rcBuildCompactHeightfield(&bc,
                                   cfg.walkableHeight, cfg.walkableClimb,
                                   *solid, *chf))
    {
        rcFreeCompactHeightfield(chf);
        rcFreeHeightField(solid);
        return false;
    }

    rcFreeHeightField(solid);

    if (!rcErodeWalkableArea(&bc, cfg.walkableRadius, *chf))
    {
        rcFreeCompactHeightfield(chf);
        return false;
    }

    if (!rcBuildDistanceField(&bc, *chf))
    {
        rcFreeCompactHeightfield(chf);
        return false;
    }

    if (!rcBuildRegions(&bc, *chf,
                        0, cfg.minRegionArea, cfg.mergeRegionArea))
    {
        rcFreeCompactHeightfield(chf);
        return false;
    }

    // Contours
    rcContourSet* cset = rcAllocContourSet();
    if (!cset)
    {
        rcFreeCompactHeightfield(chf);
        return false;
    }

    if (!rcBuildContours(&bc, *chf, cfg.maxSimplificationError,
                         cfg.maxEdgeLen, *cset))
    {
        rcFreeContourSet(cset);
        rcFreeCompactHeightfield(chf);
        return false;
    }

    // PolyMesh
    rcPolyMesh* pmesh = rcAllocPolyMesh();
    if (!pmesh)
    {
        rcFreeContourSet(cset);
        rcFreeCompactHeightfield(chf);
        return false;
    }

    if (!rcBuildPolyMesh(&bc, *cset, cfg.maxVertsPerPoly, *pmesh))
    {
        rcFreePolyMesh(pmesh);
        rcFreeContourSet(cset);
        rcFreeCompactHeightfield(chf);
        return false;
    }

    // PolyMeshDetail
    rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
    if (!dmesh)
    {
        rcFreePolyMesh(pmesh);
        rcFreeContourSet(cset);
        rcFreeCompactHeightfield(chf);
        return false;
    }

    if (!rcBuildPolyMeshDetail(&bc, *pmesh, *chf,
                               cfg.detailSampleDist,
                               cfg.detailSampleMaxError,
                               *dmesh))
    {
        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);
        rcFreeContourSet(cset);
        rcFreeCompactHeightfield(chf);
        return false;
    }

    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);

    if (pmesh->nverts == 0 || pmesh->npolys == 0)
    {
        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);
        return true;
    }

    // =====================================================================
    // 4) Converter para Detour
    // =====================================================================
    std::vector<unsigned short> polyFlags(pmesh->npolys, 1);

    dtNavMeshCreateParams params{};
    params.verts = pmesh->verts;
    params.vertCount = pmesh->nverts;
    params.polys = pmesh->polys;
    params.polyAreas = pmesh->areas;
    params.polyFlags = polyFlags.data();
    params.polyCount = pmesh->npolys;
    params.nvp = pmesh->nvp;

    params.detailMeshes = dmesh->meshes;
    params.detailVerts  = dmesh->verts;
    params.detailVertsCount = dmesh->nverts;
    params.detailTris   = dmesh->tris;
    params.detailTriCount = dmesh->ntris;

    params.walkableHeight = def.navDef_m_agentHeight;
    params.walkableRadius = def.navDef_m_agentRadius;
    params.walkableClimb  = def.navDef_m_agentMaxClimb;

    rcVcopy(params.bmin, tileBMin);
    rcVcopy(params.bmax, tileBMax);

    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.tileX = tx;
    params.tileY = tz;
    params.tileLayer = 0;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        printf("[Tiles] dtCreateNavMeshData falhou no tile %d,%d\n", tx, tz);
        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);
        return false;
    }

    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);

    if (!rebuildDetourData)
    {
        dtFree(navData);
        return true;
    }

    dtStatus status =
        ctx->navMesh->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, nullptr);

    if (dtStatusFailed(status))
    {
        printf("[Tiles] addTile falhou (%d,%d)\n", tx, tz);
        dtFree(navData);
        return false;
    }

    return true;
}


// =======================================================================
// BUILD TILES IN WORLD BOUNDS
// =======================================================================
bool GtaNavTiles::BuildTilesInBounds(
    NavMeshContext* ctx,
    const float worldBMin[3],
    const float worldBMax[3])
{
    int tx0, tz0, tx1, tz1;
    if (!CalcTileRangeForAABB(ctx, worldBMin, worldBMax, tx0, tz0, tx1, tz1))
        return false;

    for (int tz = tz0; tz <= tz1; tz++)
        for (int tx = tx0; tx <= tx1; tx++)
        {
            float tbmin[3], tbmax[3];
            CalcTileBounds(ctx, tx, tz, tbmin, tbmax);
            BuildTile(ctx, tx, tz, tbmin, tbmax);
        }

    return true;
}


// =======================================================================
// REBUILD TILES AFFECTED BY DYNAMIC GEOMETRY
// =======================================================================
bool GtaNavTiles::RebuildTilesAffectedByDynamic(NavMeshContext* ctx)
{
    if (ctx->dynamicVerts.empty())
        return true;

    int tx0, tz0, tx1, tz1;
    if (!CalcTileRangeForAABB(ctx,
                              ctx->dynamicBMin, ctx->dynamicBMax,
                              tx0, tz0, tx1, tz1))
        return false;

    for (int tz = tz0; tz <= tz1; tz++)
        for (int tx = tx0; tx <= tx1; tx++)
        {
            float tbmin[3], tbmax[3];
            CalcTileBounds(ctx, tx, tz, tbmin, tbmax);
            BuildTile(ctx, tx, tz, tbmin, tbmax);
        }

    return true;
}


// =======================================================================
// BUILD TILES AROUND POSITION
// =======================================================================
bool GtaNavTiles::BuildTilesAroundPosition(
    NavMeshContext* ctx,
    const float pos[3],
    float radiusMeters)
{
    float bmin[3], bmax[3];

    bmin[0] = pos[0] - radiusMeters;
    bmin[2] = pos[2] - radiusMeters;
    bmax[0] = pos[0] + radiusMeters;
    bmax[2] = pos[2] + radiusMeters;

    // Y usa AABB global
    bmin[1] = ctx->geom->getMeshBoundsMin()[1];
    bmax[1] = ctx->geom->getMeshBoundsMax()[1];

    return BuildTilesInBounds(ctx, bmin, bmax);
}
