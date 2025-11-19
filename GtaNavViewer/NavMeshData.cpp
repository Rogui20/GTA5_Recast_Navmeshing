#include "NavMeshData.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <cstdio>
#include <cstring>

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
                                const std::vector<unsigned int>& idxIn)
{
    if (vertsIn.empty() || idxIn.empty())
    {
        printf("[NavMeshData] BuildFromMesh: malha vazia.\n");
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
    float bmin[3] = { verts[0], verts[1], verts[2] };
    float bmax[3] = { verts[0], verts[1], verts[2] };
    for (int i = 1; i < nverts; ++i)
    {
        const float* v = &verts[i*3];
        if (v[0] < bmin[0]) bmin[0] = v[0];
        if (v[1] < bmin[1]) bmin[1] = v[1];
        if (v[2] < bmin[2]) bmin[2] = v[2];
        if (v[0] > bmax[0]) bmax[0] = v[0];
        if (v[1] > bmax[1]) bmax[1] = v[1];
        if (v[2] > bmax[2]) bmax[2] = v[2];
    }

    // --- 3. Configuração Recast básica ---
    rcConfig cfg{};
    cfg.cs = 1.0f;    // cell size (ajusta depois)
    cfg.ch = 0.5f;    // cell height (ajusta depois)

    cfg.walkableSlopeAngle   = 45.0f;
    cfg.walkableHeight       = (int)ceilf(2.0f / cfg.ch);
    cfg.walkableClimb        = (int)ceilf(1.0f / cfg.ch);
    cfg.walkableRadius       = (int)ceilf(0.6f / cfg.cs);
    cfg.maxEdgeLen           = (int)(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea        = (int)rcSqr(8);   // em células
    cfg.mergeRegionArea      = (int)rcSqr(20);  // em células
    cfg.maxVertsPerPoly      = 6;
    cfg.detailSampleDist     = 6.0f;
    cfg.detailSampleMaxError = 1.0f;

    rcCalcGridSize(bmin, bmax, &cfg.width, &cfg.height);
    cfg.borderSize = 0; // pra simplificar

    rcContext ctx;

    // --- 4. Altura, rasterização e filtros ---
    rcHeightfield* solid = rcAllocHeightfield();
    if (!solid)
    {
        printf("[NavMeshData] rcAllocHeightfield falhou.\n");
        return false;
    }
    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height,
                             bmin, bmax, cfg.cs, cfg.ch))
    {
        printf("[NavMeshData] rcCreateHeightfield falhou.\n");
        rcFreeHeightField(solid);
        return false;
    }

    std::vector<unsigned char> triAreas(ntris);
    memset(triAreas.data(), 0, ntris * sizeof(unsigned char));

    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle,
                            verts.data(), nverts,
                            tris.data(), ntris,
                            triAreas.data());

    rcRasterizeTriangles(&ctx,
                         verts.data(), nverts,
                         tris.data(),
                         triAreas.data(),
                         ntris,
                         *solid,
                         cfg.walkableClimb);

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    // --- 5. Compact heightfield ---
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

    // --- 6. Contours ---
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

    // --- 7. PolyMesh ---
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

    // --- 8. PolyMeshDetail ---
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

    // --- 9. Params Detour ---
    unsigned char* navData = nullptr;
    int navDataSize = 0;

    std::vector<unsigned char> polyFlags(pmesh->npolys);
    for (int i = 0; i < pmesh->npolys; ++i)
    {
        if (pmesh->areas[i] != 0)
            polyFlags[i] = 1; // walkable
        else
            polyFlags[i] = 0;
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

    params.tileX = 0;
    params.tileY = 0;
    params.tileLayer = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        printf("[NavMeshData] dtCreateNavMeshData falhou.\n");
        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);
        return false;
    }

    rcFreePolyMeshDetail(dmesh);
    rcFreePolyMesh(pmesh);

    // --- 10. Criar dtNavMesh ---
    m_nav = dtAllocNavMesh();
    if (!m_nav)
    {
        printf("[NavMeshData] dtAllocNavMesh falhou.\n");
        dtFree(navData);
        return false;
    }

    dtNavMeshParams navParams{};
    memcpy(navParams.orig, params.bmin, sizeof(float)*3);
    navParams.tileWidth  = (params.bmax[0] - params.bmin[0]);
    navParams.tileHeight = (params.bmax[2] - params.bmin[2]);
    navParams.maxTiles   = 1;
    navParams.maxPolys   = 2048; // ajusta se quiser

    if (dtStatusFailed(m_nav->init(&navParams)))
    {
        printf("[NavMeshData] m_nav->init falhou.\n");
        dtFree(navData);
        return false;
    }

    dtStatus status = m_nav->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status))
    {
        printf("[NavMeshData] addTile falhou.\n");
        dtFree(navData);
        return false;
    }

    printf("[NavMeshData] BuildFromMesh OK. Verts=%d, Tris=%d, Polys=%d\n",
           nverts, ntris, pmesh->npolys);
    return true;
}
