#include "NavMeshBuild.h"

#include <DetourNavMeshBuilder.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    bool createNavDataForConfig(const NavmeshBuildInput& input,
                                const rcConfig& cfg,
                                const std::vector<int>& triSource,
                                int tileX,
                                int tileY,
                                dtNavMeshCreateParams& outParams,
                                unsigned char*& navData,
                                int& navDataSize)
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
        if (!rcCreateHeightfield(&input.ctx, *solid, cfg.width, cfg.height,
                                 cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        {
            printf("[NavMeshData] rcCreateHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return false;
        }

        std::vector<unsigned char> triAreas(localTris);
        memset(triAreas.data(), 0, localTris * sizeof(unsigned char));

        rcMarkWalkableTriangles(&input.ctx, cfg.walkableSlopeAngle,
                                input.verts.data(), input.nverts,
                                triSource.data(), localTris,
                                triAreas.data());

        if (!rcRasterizeTriangles(&input.ctx,
                                  input.verts.data(), input.nverts,
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

        rcFilterLowHangingWalkableObstacles(&input.ctx, cfg.walkableClimb, *solid);
        rcFilterLedgeSpans(&input.ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
        rcFilterWalkableLowHeightSpans(&input.ctx, cfg.walkableHeight, *solid);

        rcCompactHeightfield* chf = rcAllocCompactHeightfield();
        if (!chf)
        {
            printf("[NavMeshData] rcAllocCompactHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return false;
        }
        if (!rcBuildCompactHeightfield(&input.ctx,
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

        rcErodeWalkableArea(&input.ctx, cfg.walkableRadius, *chf);
        rcBuildDistanceField(&input.ctx, *chf);
        rcBuildRegions(&input.ctx, *chf, cfg.borderSize,
                       cfg.minRegionArea, cfg.mergeRegionArea);

        rcContourSet* cset = rcAllocContourSet();
        if (!cset)
        {
            printf("[NavMeshData] rcAllocContourSet falhou.\n");
            rcFreeCompactHeightfield(chf);
            return false;
        }
        if (!rcBuildContours(&input.ctx, *chf,
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
        if (!rcBuildPolyMesh(&input.ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
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

        if (!rcBuildPolyMeshDetail(&input.ctx, *pmesh, *chf,
                                   cfg.detailSampleDist, cfg.detailSampleMaxError,
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

        std::vector<float> offmeshVerts;
        std::vector<float> offmeshRads;
        std::vector<unsigned char> offmeshDirs;
        std::vector<unsigned char> offmeshAreas;
        std::vector<unsigned short> offmeshFlags;
        std::vector<unsigned int> offmeshIds;

        if (input.offmeshLinks && !input.offmeshLinks->empty())
        {
            offmeshVerts.reserve(input.offmeshLinks->size() * 6);
            offmeshRads.reserve(input.offmeshLinks->size());
            offmeshDirs.reserve(input.offmeshLinks->size());
            offmeshAreas.reserve(input.offmeshLinks->size());
            offmeshFlags.reserve(input.offmeshLinks->size());
            offmeshIds.reserve(input.offmeshLinks->size());

            unsigned int baseId = 1000;
            for (const auto& link : *input.offmeshLinks)
            {
                offmeshVerts.push_back(link.start.x);
                offmeshVerts.push_back(link.start.y);
                offmeshVerts.push_back(link.start.z);
                offmeshVerts.push_back(link.end.x);
                offmeshVerts.push_back(link.end.y);
                offmeshVerts.push_back(link.end.z);

                offmeshRads.push_back(link.radius);
                offmeshDirs.push_back(link.bidirectional ? 1 : 0);
                offmeshAreas.push_back(RC_WALKABLE_AREA);
                offmeshFlags.push_back(1);
                offmeshIds.push_back(baseId++);
            }

            params.offMeshConVerts = offmeshVerts.data();
            params.offMeshConRad = offmeshRads.data();
            params.offMeshConDir = offmeshDirs.data();
            params.offMeshConAreas = offmeshAreas.data();
            params.offMeshConFlags = offmeshFlags.data();
            params.offMeshConUserID = offmeshIds.data();
            params.offMeshConCount = static_cast<int>(offmeshDirs.size());
        }

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
    }
}

bool BuildSingleNavMesh(const NavmeshBuildInput& input,
                        const NavmeshGenerationSettings& settings,
                        dtNavMesh*& outNav)
{
    rcConfig cfg = input.baseCfg;
    cfg.borderSize = cfg.walkableRadius + 3;
    rcVcopy(cfg.bmin, input.meshBMin);
    rcVcopy(cfg.bmax, input.meshBMax);
    cfg.width  += cfg.borderSize * 2;
    cfg.height += cfg.borderSize * 2;
    cfg.bmin[0] -= cfg.borderSize * cfg.cs;
    cfg.bmin[2] -= cfg.borderSize * cfg.cs;
    cfg.bmax[0] += cfg.borderSize * cfg.cs;
    cfg.bmax[2] += cfg.borderSize * cfg.cs;

    printf("[NavMeshData] BuildFromMesh (single tile): Verts=%d, Tris=%d, Bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f) Grid=%d x %d (border=%d)\n",
           input.nverts, input.ntris,
           input.meshBMin[0], input.meshBMin[1], input.meshBMin[2],
           input.meshBMax[0], input.meshBMax[1], input.meshBMax[2],
           cfg.width, cfg.height, cfg.borderSize);

    dtNavMeshCreateParams params{};
    unsigned char* navMeshData = nullptr;
    int navMeshDataSize = 0;
    if (!createNavDataForConfig(input, cfg, input.tris, 0, 0, params, navMeshData, navMeshDataSize))
        return false;

    dtNavMesh* nav = dtAllocNavMesh();
    if (!nav)
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

    if (dtStatusFailed(nav->init(&navParams)))
    {
        printf("[NavMeshData] m_nav->init falhou.\n");
        dtFree(navMeshData);
        dtFreeNavMesh(nav);
        return false;
    }

    dtStatus status = nav->addTile(navMeshData, navMeshDataSize, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status))
    {
        printf("[NavMeshData] addTile falhou. status=0x%x size=%d\n", status, navMeshDataSize);
        dtFree(navMeshData);
        dtFreeNavMesh(nav);
        return false;
    }

    printf("[NavMeshData] BuildFromMesh OK (single tile). Verts=%d, Tris=%d, Polys=%d\n",
           input.nverts, input.ntris, params.polyCount);

    outNav = nav;
    return true;
}