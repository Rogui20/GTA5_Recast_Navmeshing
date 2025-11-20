#include "NavMeshData.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>

NavMeshData::~NavMeshData()
{
    if (m_nav)
    {
        dtFreeNavMesh(m_nav);
        m_nav = nullptr;
    }
}

bool NavMeshData::Load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("[NavMeshData] Failed to open: %s\n", path);
        return false;
    }

    int magic = 0, version = 0;
    fread(&magic, sizeof(int), 1, f);
    fread(&version, sizeof(int), 1, f);

    if (magic != 'msNV')
    {
        printf("[NavMeshData] Invalid magic\n");
        fclose(f);
        return false;
    }

    dtNavMeshParams params;
    fread(&params, sizeof(params), 1, f);

    m_nav = dtAllocNavMesh();
    if (!m_nav)
    {
        printf("[NavMeshData] dtAllocNavMesh failed\n");
        fclose(f);
        return false;
    }

    if (dtStatusFailed(m_nav->init(&params)))
    {
        printf("[NavMeshData] init(params) failed\n");
        fclose(f);
        return false;
    }

    for (int i = 0; i < params.maxTiles; i++)
    {
        int dataSize = 0;
        fread(&dataSize, sizeof(int), 1, f);

        if (dataSize == 0)
            continue;

        unsigned char* data = (unsigned char*)dtAlloc(dataSize, DT_ALLOC_PERM);
        fread(data, dataSize, 1, f);

        if (dtStatusFailed(m_nav->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, NULL)))
        {
            dtFree(data);
        }
    }

    fclose(f);
    printf("[NavMeshData] Loaded OK\n");
    return true;
}


// ----------------------------------------------------------------------------
// ExtractDebugMesh()
// Converte polys em triângulos para renderizar no ViewerGL
// ----------------------------------------------------------------------------
void NavMeshData::ExtractDebugMesh(
    std::vector<glm::vec3>& outVerts,
    std::vector<glm::vec3>& outLines )
{
    outVerts.clear();
    outLines.clear();

    if (!m_nav)
        return;

    for (int t = 0; t < m_nav->getMaxTiles(); t++)
    {
        const dtMeshTile* tile = m_nav->getTile(t);
        if (!tile || !tile->header) continue;

        const dtPoly* polys = tile->polys;
        const float* verts = tile->verts;
        const dtMeshHeader* header = tile->header;

        for (int i = 0; i < header->polyCount; i++)
        {
            const dtPoly& p = polys[i];
            if (p.getType() != DT_POLYTYPE_GROUND) continue;

            for (int j = 2; j < p.vertCount; j++)
            {
                int v0 = p.verts[0];
                int v1 = p.verts[j - 1];
                int v2 = p.verts[j];

                glm::vec3 a(verts[v0 * 3 + 0], verts[v0 * 3 + 1], verts[v0 * 3 + 2]);
                glm::vec3 b(verts[v1 * 3 + 0], verts[v1 * 3 + 1], verts[v1 * 3 + 2]);
                glm::vec3 c(verts[v2 * 3 + 0], verts[v2 * 3 + 1], verts[v2 * 3 + 2]);

                outVerts.push_back(a);
                outVerts.push_back(b);
                outVerts.push_back(c);

                outLines.push_back(a);
                outLines.push_back(b);

                outLines.push_back(b);
                outLines.push_back(c);

                outLines.push_back(c);
                outLines.push_back(a);
            }
        }
    }
    printf("ExtractDebugMesh: verts=%zu  lines=%zu\n",
    outVerts.size(), outLines.size());

}

bool NavMeshData::BuildFromMesh(const std::vector<glm::vec3>& vertsIn,
                                const std::vector<unsigned int>& idxIn,
                                const NavmeshGenerationSettings& settings)
{
    struct LoggingRcContext : public rcContext
    {
        void doLog(const rcLogCategory category, const char* msg, const int len) override
        {
            rcIgnoreUnused(len);
            const char* prefix = "[Recast]";
            switch (category)
            {
            case RC_LOG_PROGRESS: prefix = "[Recast][info]"; break;
            case RC_LOG_WARNING:  prefix = "[Recast][warn]"; break;
            case RC_LOG_ERROR:    prefix = "[Recast][error]"; break;
            default: break;
            }

            printf("%s %s\n", prefix, msg);
        }
    };

    LoggingRcContext ctx;

    if (vertsIn.empty() || idxIn.empty())
    {
        printf("[NavMeshData] BuildFromMesh: malha vazia. Verts=%zu, Indices=%zu\n",
               vertsIn.size(), idxIn.size());
        return false;
    }

    if (idxIn.size() % 3 != 0)
    {
        printf("[NavMeshData] BuildFromMesh: numero de indices nao e multiplo de 3 (%zu).\n",
               idxIn.size());
        return false;
    }

    // Se já existe um navmesh, limpa
    if (m_nav)
    {
        dtFreeNavMesh(m_nav);
        m_nav = nullptr;
    }

    // --- 1. Flatten vertices e indices ---
    const int nverts = (int)vertsIn.size();
    const int ntris  = (int)(idxIn.size() / 3);

    // valida indices
    for (size_t i = 0; i < idxIn.size(); ++i)
    {
        if (idxIn[i] >= vertsIn.size())
        {
            printf("[NavMeshData] BuildFromMesh: indice fora do limite na pos %zu (idx=%u, verts=%zu).\n",
                   i, idxIn[i], vertsIn.size());
            return false;
        }
    }

    std::vector<float> verts(nverts * 3);
    for (int i = 0; i < nverts; ++i)
    {
        verts[i*3+0] = vertsIn[i].x;
        verts[i*3+1] = vertsIn[i].y;
        verts[i*3+2] = vertsIn[i].z;
    }

    std::vector<int> tris(ntris * 3);
    for (int i = 0; i < ntris * 3; ++i)
        tris[i] = (int)idxIn[i];

    // --- 2. Bounds da malha ---
    float meshBMin[3] = { verts[0], verts[1], verts[2] };
    float meshBMax[3] = { verts[0], verts[1], verts[2] };
    for (int i = 1; i < nverts; ++i)
    {
        const float* v = &verts[i*3];
        if (v[0] < meshBMin[0]) meshBMin[0] = v[0];
        if (v[1] < meshBMin[1]) meshBMin[1] = v[1];
        if (v[2] < meshBMin[2]) meshBMin[2] = v[2];
        if (v[0] > meshBMax[0]) meshBMax[0] = v[0];
        if (v[1] > meshBMax[1]) meshBMax[1] = v[1];
        if (v[2] > meshBMax[2]) meshBMax[2] = v[2];
    }

    auto fillBaseConfig = [&](rcConfig& cfg)
    {
        cfg = {};
        cfg.cs = std::max(0.01f, settings.cellSize);
        cfg.ch = std::max(0.01f, settings.cellHeight);

        cfg.walkableSlopeAngle   = settings.agentMaxSlope;
        cfg.walkableHeight       = (int)ceilf(settings.agentHeight / cfg.ch);
        cfg.walkableClimb        = (int)floorf(settings.agentMaxClimb / cfg.ch);
        cfg.walkableRadius       = (int)ceilf(settings.agentRadius / cfg.cs);
        cfg.maxEdgeLen           = std::max(0, (int)std::ceil(settings.maxEdgeLen / cfg.cs));
        cfg.maxSimplificationError = settings.maxSimplificationError;
        cfg.minRegionArea        = (int)rcSqr(std::max(0.0f, settings.minRegionSize));
        cfg.mergeRegionArea      = (int)rcSqr(std::max(0.0f, settings.mergeRegionSize));
        cfg.maxVertsPerPoly      = std::max(3, settings.maxVertsPerPoly);
        cfg.detailSampleDist     = std::max(0.0f, settings.detailSampleDist);
        cfg.detailSampleMaxError = std::max(0.0f, settings.detailSampleMaxError);
    };

    rcConfig baseCfg{};
    fillBaseConfig(baseCfg);

    rcCalcGridSize(meshBMin, meshBMax, baseCfg.cs, &baseCfg.width, &baseCfg.height);
    if (baseCfg.width == 0 || baseCfg.height == 0)
    {
        printf("[NavMeshData] BuildFromMesh: rcCalcGridSize retornou grade invalida (%d x %d).\n",
               baseCfg.width, baseCfg.height);
        return false;
    }

    auto createNavDataForConfig = [&](const rcConfig& cfg,
                                      const std::vector<int>& triSource,
                                      int tileX,
                                      int tileY,
                                      dtNavMeshCreateParams& outParams,
                                      unsigned char*& navData,
                                      int& navDataSize) -> bool
    {
        const int localTris = (int)(triSource.size() / 3);
        if (localTris == 0)
            return false;

        rcHeightfield* solid = rcAllocHeightfield();
        if (!solid)
        {
            printf("[NavMeshData] rcAllocHeightfield falhou.\n");
            return false;
        }
        if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height,
                                 cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        {
            printf("[NavMeshData] rcCreateHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return false;
        }

        std::vector<unsigned char> triAreas(localTris);
        memset(triAreas.data(), 0, localTris * sizeof(unsigned char));

        rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle,
                                verts.data(), nverts,
                                triSource.data(), localTris,
                                triAreas.data());

        if (!rcRasterizeTriangles(&ctx,
                                  verts.data(), nverts,
                                  triSource.data(),
                                  triAreas.data(),
                                  localTris,
                                  *solid,
                                  cfg.walkableClimb))
        {
            printf("[NavMeshData] rcRasterizeTriangles falhou.\n");
            rcFreeHeightField(solid);
            return false;
        }

        rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
        rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
        rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

        rcCompactHeightfield* chf = rcAllocCompactHeightfield();
        if (!chf)
        {
            printf("[NavMeshData] rcAllocCompactHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return false;
        }
        if (!rcBuildCompactHeightfield(&ctx,
                                       cfg.walkableHeight, cfg.walkableClimb,
                                       *solid, *chf))
        {
            printf("[NavMeshData] rcBuildCompactHeightfield falhou.\n");
            rcFreeCompactHeightfield(chf);
            rcFreeHeightField(solid);
            return false;
        }

        rcFreeHeightField(solid);
        solid = nullptr;

        rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf);
        rcBuildDistanceField(&ctx, *chf);
        rcBuildRegions(&ctx, *chf, cfg.borderSize,
                       cfg.minRegionArea, cfg.mergeRegionArea);

        rcContourSet* cset = rcAllocContourSet();
        if (!cset)
        {
            printf("[NavMeshData] rcAllocContourSet falhou.\n");
            rcFreeCompactHeightfield(chf);
            return false;
        }
        if (!rcBuildContours(&ctx, *chf,
                             cfg.maxSimplificationError,
                             cfg.maxEdgeLen,
                             *cset))
        {
            printf("[NavMeshData] rcBuildContours falhou.\n");
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return false;
        }

        rcPolyMesh* pmesh = rcAllocPolyMesh();
        if (!pmesh)
        {
            printf("[NavMeshData] rcAllocPolyMesh falhou.\n");
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return false;
        }
        if (!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
        {
            printf("[NavMeshData] rcBuildPolyMesh falhou.\n");
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return false;
        }

        rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
        if (!dmesh)
        {
            printf("[NavMeshData] rcAllocPolyMeshDetail falhou.\n");
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return false;
        }
        if (!rcBuildPolyMeshDetail(&ctx,
                                   *pmesh, *chf,
                                   cfg.detailSampleDist,
                                   cfg.detailSampleMaxError,
                                   *dmesh))
        {
            printf("[NavMeshData] rcBuildPolyMeshDetail falhou.\n");
            rcFreePolyMeshDetail(dmesh);
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return false;
        }

        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);

        std::vector<unsigned short> polyFlags(pmesh->npolys);
        for (int i = 0; i < pmesh->npolys; ++i)
        {
            polyFlags[i] = (pmesh->areas[i] != 0) ? 1 : 0;
        }

        dtNavMeshCreateParams params{};
        params.verts = pmesh->verts;
        params.vertCount = pmesh->nverts;
        params.polys = pmesh->polys;
        params.polyAreas = pmesh->areas;
        params.polyFlags = polyFlags.data();
        params.polyCount = pmesh->npolys;
        params.nvp = pmesh->nvp;
        params.detailMeshes = dmesh->meshes;
        params.detailVerts = dmesh->verts;
        params.detailVertsCount = dmesh->nverts;
        params.detailTris = dmesh->tris;
        params.detailTriCount = dmesh->ntris;
        params.walkableHeight = (float)cfg.walkableHeight * cfg.ch;
        params.walkableRadius = (float)cfg.walkableRadius * cfg.cs;
        params.walkableClimb  = (float)cfg.walkableClimb  * cfg.ch;
        params.bmin[0] = pmesh->bmin[0];
        params.bmin[1] = pmesh->bmin[1];
        params.bmin[2] = pmesh->bmin[2];
        params.bmax[0] = pmesh->bmax[0];
        params.bmax[1] = pmesh->bmax[1];
        params.bmax[2] = pmesh->bmax[2];
        params.cs = cfg.cs;
        params.ch = cfg.ch;
        params.buildBvTree = true;
        params.tileX = tileX;
        params.tileY = tileY;
        params.tileLayer = 0;

        bool ok = dtCreateNavMeshData(&params, &navData, &navDataSize);
        if (!ok)
        {
            printf("[NavMeshData] dtCreateNavMeshData falhou. polys=%d verts=%d detailVerts=%d detailTris=%d bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
                   pmesh->npolys, pmesh->nverts, dmesh->nverts, dmesh->ntris,
                   params.bmin[0], params.bmin[1], params.bmin[2],
                   params.bmax[0], params.bmax[1], params.bmax[2]);
        }

        outParams = params;

        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);

        return ok;
    };

    if (settings.mode == NavmeshBuildMode::SingleMesh)
    {
        rcConfig cfg = baseCfg;
        cfg.borderSize = cfg.walkableRadius + 3;
        rcVcopy(cfg.bmin, meshBMin);
        rcVcopy(cfg.bmax, meshBMax);
        cfg.width  += cfg.borderSize * 2;
        cfg.height += cfg.borderSize * 2;
        cfg.bmin[0] -= cfg.borderSize * cfg.cs;
        cfg.bmin[2] -= cfg.borderSize * cfg.cs;
        cfg.bmax[0] += cfg.borderSize * cfg.cs;
        cfg.bmax[2] += cfg.borderSize * cfg.cs;

        printf("[NavMeshData] BuildFromMesh (single tile): Verts=%d, Tris=%d, Bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f) Grid=%d x %d (border=%d)\n",
               nverts, ntris,
               meshBMin[0], meshBMin[1], meshBMin[2],
               meshBMax[0], meshBMax[1], meshBMax[2],
               cfg.width, cfg.height, cfg.borderSize);

        dtNavMeshCreateParams params{};
        unsigned char* navMeshData = nullptr;
        int navMeshDataSize = 0;
        if (!createNavDataForConfig(cfg, tris, 0, 0, params, navMeshData, navMeshDataSize))
            return false;

        m_nav = dtAllocNavMesh();
        if (!m_nav)
        {
            printf("[NavMeshData] dtAllocNavMesh falhou.\n");
            dtFree(navMeshData);
            return false;
        }

        dtNavMeshParams navParams{};
        memcpy(navParams.orig, params.bmin, sizeof(float)*3);
        navParams.tileWidth  = (params.bmax[0] - params.bmin[0]);
        navParams.tileHeight = (params.bmax[2] - params.bmin[2]);
        navParams.maxTiles   = 1;
        navParams.maxPolys   = 2048;

        if (dtStatusFailed(m_nav->init(&navParams)))
        {
            printf("[NavMeshData] m_nav->init falhou.\n");
            dtFree(navMeshData);
            return false;
        }

        dtStatus status = m_nav->addTile(navMeshData, navMeshDataSize, DT_TILE_FREE_DATA, 0, nullptr);
        if (dtStatusFailed(status))
        {
            printf("[NavMeshData] addTile falhou. status=0x%x size=%d\n", status, navMeshDataSize);
            dtFree(navMeshData);
            return false;
        }

        printf("[NavMeshData] BuildFromMesh OK (single tile). Verts=%d, Tris=%d, Polys=%d\n",
               nverts, ntris, params.polyCount);
        return true;
    }
    else
    {
        rcConfig cfg = baseCfg;
        cfg.borderSize = cfg.walkableRadius + 3;
        cfg.tileSize = std::max(1, settings.tileSize);

        const int tileWidthCount = (cfg.width + cfg.tileSize - 1) / cfg.tileSize;
        const int tileHeightCount = (cfg.height + cfg.tileSize - 1) / cfg.tileSize;

        if (tileWidthCount <= 0 || tileHeightCount <= 0)
        {
            printf("[NavMeshData] BuildFromMesh (tiled): contagem de tiles invalida (%d x %d).\n",
                   tileWidthCount, tileHeightCount);
            return false;
        }

        printf("[NavMeshData] BuildFromMesh (tiled): Tiles=%d x %d (tileSize=%d, border=%d) Bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
               tileWidthCount, tileHeightCount, cfg.tileSize, cfg.borderSize,
               meshBMin[0], meshBMin[1], meshBMin[2],
               meshBMax[0], meshBMax[1], meshBMax[2]);

        m_nav = dtAllocNavMesh();
        if (!m_nav)
        {
            printf("[NavMeshData] dtAllocNavMesh falhou.\n");
            return false;
        }

        dtNavMeshParams navParams{};
        rcVcopy(navParams.orig, meshBMin);
        navParams.tileWidth = cfg.tileSize * cfg.cs;
        navParams.tileHeight = cfg.tileSize * cfg.cs;
        navParams.maxTiles = tileWidthCount * tileHeightCount;
        navParams.maxPolys = 2048;

        if (dtStatusFailed(m_nav->init(&navParams)))
        {
            printf("[NavMeshData] m_nav->init falhou (tiled).\n");
            dtFreeNavMesh(m_nav);
            m_nav = nullptr;
            return false;
        }

        bool builtAnyTile = false;

        auto overlapsBounds = [](const float* amin, const float* amax, const float* bmin, const float* bmax)
        {
            if (amin[0] > bmax[0] || amax[0] < bmin[0]) return false;
            if (amin[1] > bmax[1] || amax[1] < bmin[1]) return false;
            if (amin[2] > bmax[2] || amax[2] < bmin[2]) return false;
            return true;
        };

        for (int ty = 0; ty < tileHeightCount; ++ty)
        {
            for (int tx = 0; tx < tileWidthCount; ++tx)
            {
                rcConfig tileCfg = cfg;
                tileCfg.width = cfg.tileSize + cfg.borderSize * 2;
                tileCfg.height = cfg.tileSize + cfg.borderSize * 2;

                float tbmin[3];
                float tbmax[3];
                tbmin[0] = meshBMin[0] + tx * cfg.tileSize * cfg.cs;
                tbmin[1] = meshBMin[1];
                tbmin[2] = meshBMin[2] + ty * cfg.tileSize * cfg.cs;
                tbmax[0] = meshBMin[0] + (tx + 1) * cfg.tileSize * cfg.cs;
                tbmax[1] = meshBMax[1];
                tbmax[2] = meshBMin[2] + (ty + 1) * cfg.tileSize * cfg.cs;

                rcVcopy(tileCfg.bmin, tbmin);
                rcVcopy(tileCfg.bmax, tbmax);
                tileCfg.bmax[0] = std::min(tileCfg.bmax[0], meshBMax[0]);
                tileCfg.bmax[2] = std::min(tileCfg.bmax[2], meshBMax[2]);

                tileCfg.bmin[0] -= cfg.borderSize * cfg.cs;
                tileCfg.bmin[2] -= cfg.borderSize * cfg.cs;
                tileCfg.bmax[0] += cfg.borderSize * cfg.cs;
                tileCfg.bmax[2] += cfg.borderSize * cfg.cs;

                std::vector<int> tileTris;
                tileTris.reserve(tris.size());

                for (int i = 0; i < ntris; ++i)
                {
                    const float* v0 = &verts[tris[i*3+0] * 3];
                    const float* v1 = &verts[tris[i*3+1] * 3];
                    const float* v2 = &verts[tris[i*3+2] * 3];

                    float triMin[3] = {
                        std::min({v0[0], v1[0], v2[0]}),
                        std::min({v0[1], v1[1], v2[1]}),
                        std::min({v0[2], v1[2], v2[2]})
                    };
                    float triMax[3] = {
                        std::max({v0[0], v1[0], v2[0]}),
                        std::max({v0[1], v1[1], v2[1]}),
                        std::max({v0[2], v1[2], v2[2]})
                    };

                    if (overlapsBounds(triMin, triMax, tileCfg.bmin, tileCfg.bmax))
                    {
                        tileTris.push_back(tris[i*3+0]);
                        tileTris.push_back(tris[i*3+1]);
                        tileTris.push_back(tris[i*3+2]);
                    }
                }

                if (tileTris.empty())
                    continue;

                dtNavMeshCreateParams params{};
                unsigned char* navMeshData = nullptr;
                int navMeshDataSize = 0;
                if (!createNavDataForConfig(tileCfg, tileTris, tx, ty, params, navMeshData, navMeshDataSize))
                {
                    dtFreeNavMesh(m_nav);
                    m_nav = nullptr;
                    return false;
                }

                dtStatus status = m_nav->addTile(navMeshData, navMeshDataSize, DT_TILE_FREE_DATA, 0, nullptr);
                if (dtStatusFailed(status))
                {
                    printf("[NavMeshData] addTile falhou (tile %d,%d). status=0x%x size=%d\n", tx, ty, status, navMeshDataSize);
                    dtFree(navMeshData);
                    dtFreeNavMesh(m_nav);
                    m_nav = nullptr;
                    return false;
                }

                builtAnyTile = true;
            }
        }

        if (!builtAnyTile)
        {
            printf("[NavMeshData] Nenhum tile de navmesh foi gerado.\n");
            dtFreeNavMesh(m_nav);
            m_nav = nullptr;
            return false;
        }

        printf("[NavMeshData] BuildFromMesh OK (tiled). Tiles=%d x %d\n",
               tileWidthCount, tileHeightCount);
        return true;
    }
}
