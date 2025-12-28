#include "NavMeshBuild.h"

#include <DetourMath.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <unordered_map>

namespace
{
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

#if DEBUG_MODE
#define DEBUG_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) do { } while (0)
#endif

    bool overlapsBounds(const float* amin, const float* amax, const float* bmin, const float* bmax)
    {
        if (amin[0] > bmax[0] || amax[0] < bmin[0]) return false;
        if (amin[1] > bmax[1] || amax[1] < bmin[1]) return false;
        if (amin[2] > bmax[2] || amax[2] < bmin[2]) return false;
        return true;
    }

    static constexpr uint32_t TILE_CACHE_MAGIC   = 'G' << 24 | 'T' << 16 | 'A' << 8 | 'V';
    static constexpr uint32_t TILE_CACHE_VERSION = 1;
    static constexpr const char* DEFAULT_CACHE_PATH = "navmesh_tilecache.bin";

    struct DiskTileHeader
    {
        int      tx;
        int      ty;
        uint32_t dataSize;
        uint64_t geomHash;
    };

    struct TileCacheFileHeader
    {
        uint32_t        magic;
        uint32_t        version;
        uint32_t        tileCount;
        dtNavMeshParams navParams;
    };

    uint64_t fnv1a64(const void* data, size_t size)
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    uint64_t computeTileHash(const NavmeshBuildInput& input, const std::vector<int>& tileTris)
    {
        std::vector<int> uniqueIdx = tileTris;
        std::sort(uniqueIdx.begin(), uniqueIdx.end());
        uniqueIdx.erase(std::unique(uniqueIdx.begin(), uniqueIdx.end()), uniqueIdx.end());

        std::vector<float> tileVerts;
        tileVerts.reserve(uniqueIdx.size() * 3);
        for (int idx : uniqueIdx)
        {
            const float* v = &input.verts[idx * 3];
            tileVerts.push_back(v[0]);
            tileVerts.push_back(v[1]);
            tileVerts.push_back(v[2]);
        }

        uint64_t hash = 0xcbf29ce484222325ULL;
        if (!tileVerts.empty())
        {
            hash ^= fnv1a64(tileVerts.data(), tileVerts.size() * sizeof(float));
            hash *= 0x100000001b3ULL;
        }
        if (!tileTris.empty())
        {
            hash ^= fnv1a64(tileTris.data(), tileTris.size() * sizeof(int));
            hash *= 0x100000001b3ULL;
        }

        return hash;
    }

    uint64_t makeTileKey(int tx, int ty)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
    }

    enum class NavTileBuildResult
    {
        Success,
        Empty,
        Error,
    };

    NavTileBuildResult createNavDataForConfig(const NavmeshBuildInput& input,
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
            return NavTileBuildResult::Empty;

        rcHeightfield* solid = rcAllocHeightfield();
        if (!solid)
        {
            printf("[NavMeshData] rcAllocHeightfield falhou.\n");
            return NavTileBuildResult::Error;
        }
        if (!rcCreateHeightfield(&input.ctx, *solid, cfg.width, cfg.height,
                                 cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        {
            printf("[NavMeshData] rcCreateHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return NavTileBuildResult::Error;
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
            return NavTileBuildResult::Error;
        }

        rcFilterLowHangingWalkableObstacles(&input.ctx, cfg.walkableClimb, *solid);
        rcFilterLedgeSpans(&input.ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
        rcFilterWalkableLowHeightSpans(&input.ctx, cfg.walkableHeight, *solid);

        rcCompactHeightfield* chf = rcAllocCompactHeightfield();
        if (!chf)
        {
            printf("[NavMeshData] rcAllocCompactHeightfield falhou.\n");
            rcFreeHeightField(solid);
            return NavTileBuildResult::Error;
        }
        if (!rcBuildCompactHeightfield(&input.ctx,
                                       cfg.walkableHeight, cfg.walkableClimb,
                                       *solid, *chf))
        {
            printf("[NavMeshData] rcBuildCompactHeightfield falhou.\n");
            rcFreeCompactHeightfield(chf);
            rcFreeHeightField(solid);
            return NavTileBuildResult::Error;
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
            return NavTileBuildResult::Error;
        }
        if (!rcBuildContours(&input.ctx, *chf,
                             cfg.maxSimplificationError,
                             cfg.maxEdgeLen,
                             *cset))
        {
            printf("[NavMeshData] rcBuildContours falhou.\n");
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return NavTileBuildResult::Error;
        }

        rcPolyMesh* pmesh = rcAllocPolyMesh();
        if (!pmesh)
        {
            printf("[NavMeshData] rcAllocPolyMesh falhou.\n");
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return NavTileBuildResult::Error;
        }
        if (!rcBuildPolyMesh(&input.ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
        {
            printf("[NavMeshData] rcBuildPolyMesh falhou.\n");
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return NavTileBuildResult::Error;
        }

        if (pmesh->npolys == 0)
        {
            DEBUG_LOG("[NavMeshData] Tile %d,%d nao possui polys apos filtros. Verts=%d Tris=%d Contours=%d\n",
                      tileX, tileY, pmesh->nverts, localTris, cset->nconts);
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return NavTileBuildResult::Empty;
        }

        rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
        if (!dmesh)
        {
            printf("[NavMeshData] rcAllocPolyMeshDetail falhou.\n");
            rcFreePolyMesh(pmesh);
            rcFreeContourSet(cset);
            rcFreeCompactHeightfield(chf);
            return NavTileBuildResult::Error;
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
            return NavTileBuildResult::Error;
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
                offmeshAreas.push_back(link.area);
                offmeshFlags.push_back(link.flags);
                offmeshIds.push_back(link.userId != 0 ? link.userId : baseId++);
            }

            params.offMeshConVerts = offmeshVerts.data();
            params.offMeshConRad = offmeshRads.data();
            params.offMeshConDir = offmeshDirs.data();
            params.offMeshConAreas = offmeshAreas.data();
            params.offMeshConFlags = offmeshFlags.data();
            params.offMeshConUserID = offmeshIds.data();
            params.offMeshConCount = static_cast<int>(offmeshDirs.size());
        }

        printf("[NavMeshData] params.offMeshConCount %d \n", params.offMeshConCount);
        bool ok = dtCreateNavMeshData(&params, &navData, &navDataSize);
        if (!ok)
        {
            printf("[NavMeshData] dtCreateNavMeshData falhou. polys=%d verts=%d detailVerts=%d detailTris=%d bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
                   pmesh->npolys, pmesh->nverts, dmesh->nverts, dmesh->ntris,
                   params.bmin[0], params.bmin[1], params.bmin[2],
                   params.bmax[0], params.bmax[1], params.bmax[2]);
        }

        outParams = params;

        DEBUG_LOG("[NavMeshData] Tile %d,%d navData criado. polys=%d verts=%d detailVerts=%d detailTris=%d area=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
                  tileX, tileY, pmesh->npolys, pmesh->nverts, dmesh->nverts, dmesh->ntris,
                  params.bmin[0], params.bmin[1], params.bmin[2],
                  params.bmax[0], params.bmax[1], params.bmax[2]);

        rcFreePolyMeshDetail(dmesh);
        rcFreePolyMesh(pmesh);

        return ok ? NavTileBuildResult::Success : NavTileBuildResult::Error;
    }

    bool saveTileCache(const char* cachePath,
                       dtNavMesh* nav,
                       const std::unordered_map<uint64_t, uint64_t>& tileHashes)
    {
        if (!cachePath || !nav)
            return false;

        std::vector<DiskTileHeader> diskTiles;
        std::vector<const unsigned char*> tileData;
        diskTiles.reserve(nav->getMaxTiles());
        tileData.reserve(nav->getMaxTiles());

        for (int i = 0; i < nav->getMaxTiles(); ++i)
        {
            const dtMeshTile* tile = nav->getTile(i);
            if (!tile || !tile->header || !tile->data || tile->dataSize == 0)
                continue;

            DiskTileHeader th{};
            th.tx = tile->header->x;
            th.ty = tile->header->y;
            th.dataSize = tile->dataSize;
            const uint64_t key = makeTileKey(th.tx, th.ty);
            const auto itHash = tileHashes.find(key);
            th.geomHash = itHash != tileHashes.end() ? itHash->second : 0;

            diskTiles.push_back(th);
            tileData.push_back(tile->data);
        }

        TileCacheFileHeader header{};
        header.magic = TILE_CACHE_MAGIC;
        header.version = TILE_CACHE_VERSION;
        header.tileCount = static_cast<uint32_t>(diskTiles.size());
        memcpy(&header.navParams, nav->getParams(), sizeof(dtNavMeshParams));

        FILE* fp = fopen(cachePath, "wb");
        if (!fp)
            return false;

        bool ok = fwrite(&header, sizeof(TileCacheFileHeader), 1, fp) == 1;
        for (size_t i = 0; ok && i < diskTiles.size(); ++i)
        {
            ok = fwrite(&diskTiles[i], sizeof(DiskTileHeader), 1, fp) == 1;
            ok = ok && fwrite(tileData[i], diskTiles[i].dataSize, 1, fp) == 1;
        }

        fclose(fp);

        if (ok)
            printf("[NavMeshData] Cache salvo em %s (%zu tiles)\n", cachePath, diskTiles.size());

        return ok;
    }

    bool loadTileCache(const char* cachePath,
                       dtNavMesh* nav,
                       const std::unordered_map<uint64_t, uint64_t>& expectedHashes,
                       std::unordered_map<uint64_t, bool>& tilesLoaded,
                       bool& cacheDirty)
    {
        if (!cachePath || !nav)
            return false;

        FILE* fp = fopen(cachePath, "rb");
        if (!fp)
            return false;

        TileCacheFileHeader header{};
        if (fread(&header, sizeof(TileCacheFileHeader), 1, fp) != 1)
        {
            fclose(fp);
            return false;
        }

        if (header.magic != TILE_CACHE_MAGIC || header.version != TILE_CACHE_VERSION)
        {
            fclose(fp);
            printf("[NavMeshData] Cache incompatível (magic/version).\n");
            return false;
        }

        if (memcmp(&header.navParams, nav->getParams(), sizeof(dtNavMeshParams)) != 0)
        {
            fclose(fp);
            printf("[NavMeshData] Cache incompatível com parametros atuais.\n");
            return false;
        }

        bool ok = true;
        for (uint32_t i = 0; ok && i < header.tileCount; ++i)
        {
            DiskTileHeader th{};
            ok = fread(&th, sizeof(DiskTileHeader), 1, fp) == 1;
            if (!ok)
                break;

            unsigned char* data = static_cast<unsigned char*>(dtAlloc(th.dataSize, DT_ALLOC_PERM));
            if (!data)
            {
                ok = false;
                break;
            }

            ok = fread(data, th.dataSize, 1, fp) == 1;
            if (!ok)
            {
                dtFree(data);
                break;
            }

            const uint64_t key = makeTileKey(th.tx, th.ty);
            const auto itHash = expectedHashes.find(key);
            const bool hashMatch = itHash != expectedHashes.end() && itHash->second == th.geomHash;
            if (hashMatch)
            {
                dtStatus status = nav->addTile(data, th.dataSize, DT_TILE_FREE_DATA, 0, nullptr);
                if (dtStatusSucceed(status))
                {
                    tilesLoaded[key] = true;
                }
                else
                {
                    cacheDirty = true;
                    dtFree(data);
                    printf("[NavMeshData] Falha ao adicionar tile do cache (%d,%d) status=0x%x\n", th.tx, th.ty, status);
                }
            }
            else
            {
                cacheDirty = true;
                dtFree(data);
            }
        }

        fclose(fp);
        return ok;
    }
}

bool BuildTiledNavMesh(const NavmeshBuildInput& input,
                       const NavmeshGenerationSettings& settings,
                       dtNavMesh*& outNav,
                       bool buildTilesNow,
                       int* outTilesBuilt,
                       int* outTilesSkipped,
                       const std::atomic_bool* cancelFlag,
                       bool useCache,
                       const char* cachePath)
{
    auto isCancelled = [cancelFlag]()
    {
        return cancelFlag && cancelFlag->load();
    };

    rcConfig cfg = input.baseCfg;
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

    const int tileCountTotal = tileWidthCount * tileHeightCount;
    const int maxAllowedTiles = 32768;
    if (tileCountTotal > maxAllowedTiles)
    {
        printf("[NavMeshData] BuildFromMesh (tiled): quantidade de tiles (%d) excede limite seguro (%d). Ajuste tileSize ou reduza a area.\n",
               tileCountTotal, maxAllowedTiles);
        return false;
    }

    printf("[NavMeshData] BuildFromMesh (tiled): Tiles=%d x %d (tileSize=%d, border=%d) Bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
           tileWidthCount, tileHeightCount, cfg.tileSize, cfg.borderSize,
           input.meshBMin[0], input.meshBMin[1], input.meshBMin[2],
           input.meshBMax[0], input.meshBMax[1], input.meshBMax[2]);

    struct TileInput
    {
        int tx = 0;
        int ty = 0;
        rcConfig cfg{};
        std::vector<int> tris;
        uint64_t geomHash = 0;
        bool added = false;
    };

    std::vector<TileInput> tilesToBuild;
    tilesToBuild.reserve(tileWidthCount * tileHeightCount);

    for (int ty = 0; ty < tileHeightCount; ++ty)
    {
        if (isCancelled())
        {
            printf("[NavMeshData] BuildFromMesh (tiled): cancelado durante preparacao de tiles.\n");
            return false;
        }
        for (int tx = 0; tx < tileWidthCount; ++tx)
        {
            rcConfig tileCfg = cfg;
            tileCfg.width = cfg.tileSize + cfg.borderSize * 2;
            tileCfg.height = cfg.tileSize + cfg.borderSize * 2;

            float tbmin[3];
            float tbmax[3];
            tbmin[0] = input.meshBMin[0] + tx * cfg.tileSize * cfg.cs;
            tbmin[1] = input.meshBMin[1];
            tbmin[2] = input.meshBMin[2] + ty * cfg.tileSize * cfg.cs;
            tbmax[0] = input.meshBMin[0] + (tx + 1) * cfg.tileSize * cfg.cs;
            tbmax[1] = input.meshBMax[1];
            tbmax[2] = input.meshBMin[2] + (ty + 1) * cfg.tileSize * cfg.cs;

            rcVcopy(tileCfg.bmin, tbmin);
            rcVcopy(tileCfg.bmax, tbmax);
            tileCfg.bmax[0] = std::min(tileCfg.bmax[0], input.meshBMax[0]);
            tileCfg.bmax[2] = std::min(tileCfg.bmax[2], input.meshBMax[2]);

            tileCfg.bmin[0] -= cfg.borderSize * cfg.cs;
            tileCfg.bmin[2] -= cfg.borderSize * cfg.cs;
            tileCfg.bmax[0] += cfg.borderSize * cfg.cs;
            tileCfg.bmax[2] += cfg.borderSize * cfg.cs;

            std::vector<int> tileTris;
            tileTris.reserve(input.tris.size());

            for (int i = 0; i < input.ntris; ++i)
            {
                if (isCancelled())
                {
                    printf("[NavMeshData] BuildFromMesh (tiled): cancelado durante filtragem de triangulos.\n");
                    return false;
                }
                const float* v0 = &input.verts[input.tris[i*3+0] * 3];
                const float* v1 = &input.verts[input.tris[i*3+1] * 3];
                const float* v2 = &input.verts[input.tris[i*3+2] * 3];

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
                    tileTris.push_back(input.tris[i*3+0]);
                    tileTris.push_back(input.tris[i*3+1]);
                    tileTris.push_back(input.tris[i*3+2]);
                }
            }

            if (tileTris.empty())
                continue;

            const uint64_t geomHash = computeTileHash(input, tileTris);

            TileInput inputTile;
            inputTile.tx = tx;
            inputTile.ty = ty;
            inputTile.cfg = tileCfg;
            inputTile.tris = std::move(tileTris);
            inputTile.geomHash = geomHash;
            tilesToBuild.push_back(std::move(inputTile));
        }
    }

    if (tilesToBuild.empty() && buildTilesNow)
    {
        printf("[NavMeshData] Nenhum tile de navmesh foi gerado.\n");
        return false;
    }

    printf("[NavMeshData] Tiles com geometria: %zu / %d\n", tilesToBuild.size(), tileCountTotal);

    std::unordered_map<uint64_t, uint64_t> tileHashes;
    tileHashes.reserve(tilesToBuild.size() * 2);
    for (auto& tile : tilesToBuild)
    {
        const uint64_t key = makeTileKey(tile.tx, tile.ty);
        tileHashes[key] = tile.geomHash;
    }

    dtNavMesh* nav = dtAllocNavMesh();
    if (!nav)
    {
        printf("[NavMeshData] dtAllocNavMesh falhou.\n");
        return false;
    }

    dtNavMeshParams navParams{};
    rcVcopy(navParams.orig, input.meshBMin);
    navParams.tileWidth = cfg.tileSize * cfg.cs;
    navParams.tileHeight = cfg.tileSize * cfg.cs;
    navParams.maxTiles = tileCountTotal;

    const unsigned int desiredMaxPolys = 2048;
    const unsigned int tileBits = (unsigned int)dtIlog2(dtNextPow2((unsigned int)navParams.maxTiles));
    const unsigned int desiredPolyBits = (unsigned int)dtIlog2(dtNextPow2(desiredMaxPolys));
    const unsigned int maxPolyBitsAllowed = tileBits >= 22 ? 0u : (22u - tileBits);
    if (maxPolyBitsAllowed == 0)
    {
        printf("[NavMeshData] maxTiles=%u consome todos os bits de ref (tileBits=%u). Ajuste tileSize ou reduza area.\n",
               (unsigned int)navParams.maxTiles, tileBits);
        dtFreeNavMesh(nav);
        return false;
    }

    const unsigned int chosenPolyBits = std::min(desiredPolyBits, maxPolyBitsAllowed);
    navParams.maxPolys = 1u << chosenPolyBits;
    if (navParams.maxPolys != desiredMaxPolys)
    {
        printf("[NavMeshData] Clamp maxPolys para %u (tileBits=%u polyBits=%u). Numero de tiles=%u\n",
               (unsigned int)navParams.maxPolys, tileBits, chosenPolyBits, (unsigned int)navParams.maxTiles);
    }

    dtStatus initStatus = nav->init(&navParams);
    if (dtStatusFailed(initStatus))
    {
        printf("[NavMeshData] m_nav->init falhou (tiled). status=0x%x maxTiles=%d maxPolys=%d tileWidth=%.3f tileHeight=%.3f orig=(%.2f, %.2f, %.2f)\n",
               initStatus, navParams.maxTiles, navParams.maxPolys, navParams.tileWidth, navParams.tileHeight,
               navParams.orig[0], navParams.orig[1], navParams.orig[2]);
        dtFreeNavMesh(nav);
        return false;
    }

    int tilesBuilt = 0;
    int tilesSkipped = 0;

    const char* cacheFile = cachePath && cachePath[0] ? cachePath : DEFAULT_CACHE_PATH;
    std::unordered_map<uint64_t, bool> tilesLoadedFromCache;
    bool cacheDirty = false;

    if (buildTilesNow)
    {
        if (useCache && !tilesToBuild.empty())
        {
            const bool loaded = loadTileCache(cacheFile, nav, tileHashes, tilesLoadedFromCache, cacheDirty);
            if (loaded)
            {
                tilesBuilt += static_cast<int>(tilesLoadedFromCache.size());
                if (!tilesLoadedFromCache.empty())
                {
                    printf("[NavMeshData] Cache carregado de %s (%zu tiles). Dirty=%s\n",
                           cacheFile,
                           tilesLoadedFromCache.size(),
                           cacheDirty ? "true" : "false");
                }
            }
        }

        for (const TileInput& inputTile : tilesToBuild)
        {
            if (isCancelled())
            {
                printf("[NavMeshData] BuildFromMesh (tiled): cancelado durante construcao dos tiles.\n");
                dtFreeNavMesh(nav);
                return false;
            }

            const uint64_t tileKey = makeTileKey(inputTile.tx, inputTile.ty);
            if (tilesLoadedFromCache.find(tileKey) != tilesLoadedFromCache.end())
            {
                continue;
            }
            dtNavMeshCreateParams params{};
            unsigned char* navMeshData = nullptr;
            int navMeshDataSize = 0;
            const NavTileBuildResult result = createNavDataForConfig(input, inputTile.cfg, inputTile.tris, inputTile.tx, inputTile.ty, params, navMeshData, navMeshDataSize);
            if (result == NavTileBuildResult::Empty)
            {
                ++tilesSkipped;
                continue;
            }
            if (result == NavTileBuildResult::Error)
            {
                printf("[NavMeshData] createNavDataForConfig falhou (tile %d,%d). Tris=%zu Bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
                       inputTile.tx, inputTile.ty, inputTile.tris.size() / 3,
                       inputTile.cfg.bmin[0], inputTile.cfg.bmin[1], inputTile.cfg.bmin[2],
                       inputTile.cfg.bmax[0], inputTile.cfg.bmax[1], inputTile.cfg.bmax[2]);
                dtFreeNavMesh(nav);
                return false;
            }

            if (params.polyCount > navParams.maxPolys)
            {
                printf("[NavMeshData] Tile %d,%d tem %d polys > maxPolys(%d). Ajuste tileSize ou reduza area.\n",
                       inputTile.tx, inputTile.ty, params.polyCount, navParams.maxPolys);
                dtFree(navMeshData);
                dtFreeNavMesh(nav);
                return false;
            }

            dtStatus status = nav->addTile(navMeshData, navMeshDataSize, DT_TILE_FREE_DATA, 0, nullptr);
            if (dtStatusFailed(status))
            {
                printf("[NavMeshData] addTile falhou (tile %d,%d). status=0x%x size=%d polys=%d verts=%d bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
                       inputTile.tx, inputTile.ty, status, navMeshDataSize, params.polyCount, params.vertCount,
                       params.bmin[0], params.bmin[1], params.bmin[2],
                       params.bmax[0], params.bmax[1], params.bmax[2]);
                dtFree(navMeshData);
                dtFreeNavMesh(nav);
                return false;
            }

            ++tilesBuilt;
            cacheDirty = true;
        }

        printf("[NavMeshData] BuildFromMesh OK (tiled). Tiles=%d x %d (construidos=%d ignoradosSemPolys=%d)\n",
               tileWidthCount, tileHeightCount, tilesBuilt, tilesSkipped);

        if (useCache && tilesBuilt > 0)
        {
            saveTileCache(cacheFile, nav, tileHashes);
        }
    }
    else
    {
        printf("[NavMeshData] BuildFromMesh (tiled) inicializado sem construir tiles. Tiles=%d x %d (capacidade maxTiles=%d)\n",
               tileWidthCount, tileHeightCount, navParams.maxTiles);
    }

    if (outTilesBuilt) *outTilesBuilt = tilesBuilt;
    if (outTilesSkipped) *outTilesSkipped = tilesSkipped;

    outNav = nav;
    return true;
}

bool BuildSingleTile(const NavmeshBuildInput& input,
                     const NavmeshGenerationSettings& settings,
                     int tileX,
                     int tileY,
                     dtNavMesh* nav,
                     bool& outBuilt,
                     bool& outEmpty)
{
    outBuilt = false;
    outEmpty = false;

    if (!nav)
    {
        printf("[NavMeshData] BuildSingleTile: navMesh nulo.\n");
        return false;
    }

    rcConfig cfg = input.baseCfg;
    cfg.borderSize = cfg.walkableRadius + 3;
    cfg.tileSize = std::max(1, settings.tileSize);

    const int tileWidthCount = (cfg.width + cfg.tileSize - 1) / cfg.tileSize;
    const int tileHeightCount = (cfg.height + cfg.tileSize - 1) / cfg.tileSize;
    if (tileX < 0 || tileY < 0 || tileX >= tileWidthCount || tileY >= tileHeightCount)
    {
        printf("[NavMeshData] BuildSingleTile: coordenada de tile fora do grid (%d,%d) grid=%d x %d.\n",
               tileX, tileY, tileWidthCount, tileHeightCount);
        return false;
    }

    rcConfig tileCfg = cfg;
    tileCfg.width = cfg.tileSize + cfg.borderSize * 2;
    tileCfg.height = cfg.tileSize + cfg.borderSize * 2;

    float tbmin[3];
    float tbmax[3];
    tbmin[0] = input.meshBMin[0] + tileX * cfg.tileSize * cfg.cs;
    tbmin[1] = input.meshBMin[1];
    tbmin[2] = input.meshBMin[2] + tileY * cfg.tileSize * cfg.cs;
    tbmax[0] = input.meshBMin[0] + (tileX + 1) * cfg.tileSize * cfg.cs;
    tbmax[1] = input.meshBMax[1];
    tbmax[2] = input.meshBMin[2] + (tileY + 1) * cfg.tileSize * cfg.cs;

    rcVcopy(tileCfg.bmin, tbmin);
    rcVcopy(tileCfg.bmax, tbmax);
    tileCfg.bmax[0] = std::min(tileCfg.bmax[0], input.meshBMax[0]);
    tileCfg.bmax[2] = std::min(tileCfg.bmax[2], input.meshBMax[2]);

    tileCfg.bmin[0] -= cfg.borderSize * cfg.cs;
    tileCfg.bmin[2] -= cfg.borderSize * cfg.cs;
    tileCfg.bmax[0] += cfg.borderSize * cfg.cs;
    tileCfg.bmax[2] += cfg.borderSize * cfg.cs;

    std::vector<int> tileTris;
    tileTris.reserve(input.tris.size());

    for (int i = 0; i < input.ntris; ++i)
    {
        const float* v0 = &input.verts[input.tris[i*3+0] * 3];
        const float* v1 = &input.verts[input.tris[i*3+1] * 3];
        const float* v2 = &input.verts[input.tris[i*3+2] * 3];

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
            tileTris.push_back(input.tris[i*3+0]);
            tileTris.push_back(input.tris[i*3+1]);
            tileTris.push_back(input.tris[i*3+2]);
        }
    }

    dtNavMeshParams params = *nav->getParams();
    const unsigned int maxPolys = params.maxPolys;

    dtStatus removeStatus = DT_SUCCESS;
    const dtTileRef existing = nav->getTileRefAt(tileX, tileY, 0);
    if (existing)
    {
        removeStatus = nav->removeTile(existing, nullptr, nullptr);
        DEBUG_LOG("[NavMeshData] removeTile existente (%d,%d) status=0x%x\n", tileX, tileY, removeStatus);
    }

    if (tileTris.empty())
    {
        printf("[NavMeshData] BuildSingleTile: tile %d,%d nao possui geometria. Removido=%s\n",
               tileX, tileY, dtStatusSucceed(removeStatus) ? "sim" : "nao");
        outEmpty = true;
        return dtStatusSucceed(removeStatus);
    }

    dtNavMeshCreateParams createParams{};
    unsigned char* navMeshData = nullptr;
    int navMeshDataSize = 0;
    const NavTileBuildResult result = createNavDataForConfig(input, tileCfg, tileTris, tileX, tileY, createParams, navMeshData, navMeshDataSize);
    if (result == NavTileBuildResult::Empty)
    {
        outEmpty = true;
        return true;
    }
    if (result == NavTileBuildResult::Error)
    {
        return false;
    }

    if (createParams.polyCount > (int)maxPolys)
    {
        printf("[NavMeshData] BuildSingleTile: tile %d,%d tem %d polys > maxPolys(%u).\n",
               tileX, tileY, createParams.polyCount, maxPolys);
        dtFree(navMeshData);
        return false;
    }

    dtStatus addStatus = nav->addTile(navMeshData, navMeshDataSize, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(addStatus))
    {
        printf("[NavMeshData] BuildSingleTile: addTile falhou (tile %d,%d) status=0x%x size=%d polys=%d bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
               tileX, tileY, addStatus, navMeshDataSize, createParams.polyCount,
               createParams.bmin[0], createParams.bmin[1], createParams.bmin[2],
               createParams.bmax[0], createParams.bmax[1], createParams.bmax[2]);
        dtFree(navMeshData);
        return false;
    }

    outBuilt = true;
    printf("[NavMeshData] BuildSingleTile OK (%d,%d). polys=%d verts=%d bounds=(%.2f, %.2f, %.2f)-(%.2f, %.2f, %.2f)\n",
           tileX, tileY, createParams.polyCount, createParams.vertCount,
           createParams.bmin[0], createParams.bmin[1], createParams.bmin[2],
           createParams.bmax[0], createParams.bmax[1], createParams.bmax[2]);
    return true;
}
