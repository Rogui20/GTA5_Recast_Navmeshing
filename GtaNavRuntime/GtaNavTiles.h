#pragma once

#include "GtaNavContext.h"

class GtaNavTiles
{
public:

    // ================================================================
    // 1) Construir 1 tile (tx,tz)
    // ================================================================
    static bool BuildTile(
        NavMeshContext* ctx,
        int tx, int tz,
        const float tileBMin[3],
        const float tileBMax[3],
        bool rebuildDetourData = true
    );

    // ================================================================
    // 2) Rebuild de tiles dentro de um AABB
    // ================================================================
    static bool BuildTilesInBounds(
        NavMeshContext* ctx,
        const float worldBMin[3],
        const float worldBMax[3]
    );

    // ================================================================
    // 3) Rebuild apenas tiles afetados pelos PROPS
    // ================================================================
    static bool RebuildTilesAffectedByDynamic(NavMeshContext* ctx);

    // ================================================================
    // 4) Build por posição (em metros) e radius
    // ================================================================
    static bool BuildTilesAroundPosition(
        NavMeshContext* ctx,
        const float recastPos[3],
        float radiusMeters
    );

    // ================================================================
    // 5) Utilitários internos
    // ================================================================
    static void CalcTileBounds(
        const NavMeshContext* ctx,
        int tx, int tz,
        float bmin[3], float bmax[3]
    );

    static bool CalcTileRangeForAABB(
        const NavMeshContext* ctx,
        const float bmin[3],
        const float bmax[3],
        int& tx0, int& tz0,
        int& tx1, int& tz1
    );
};
