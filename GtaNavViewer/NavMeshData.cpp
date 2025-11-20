#include "NavMeshData.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourMath.h>

#include "NavMeshBuild.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace
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
}

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

bool NavMeshData::BuildTileAt(const glm::vec3& worldPos,
                              const NavmeshGenerationSettings& settings,
                              int& outTileX,
                              int& outTileY)
{
    outTileX = -1;
    outTileY = -1;

    if (!m_nav)
    {
        printf("[NavMeshData] BuildTileAt: navmesh ainda nao foi construido.\n");
        return false;
    }

    if (!m_hasTiledCache)
    {
        printf("[NavMeshData] BuildTileAt: sem cache de build tiled. Gere o navmesh tiled primeiro.\n");
        return false;
    }

    const float expectedTileWorld = m_cachedSettings.tileSize * m_cachedBaseCfg.cs;
    if (settings.mode != NavmeshBuildMode::Tiled ||
        fabsf(settings.cellSize - m_cachedSettings.cellSize) > 1e-3f ||
        fabsf(settings.cellHeight - m_cachedSettings.cellHeight) > 1e-3f ||
        settings.tileSize != m_cachedSettings.tileSize)
    {
        printf("[NavMeshData] BuildTileAt: configuracao atual difere da usada no navmesh cacheado. Use mesmos valores de tile/cell.\n");
    }

    const float tileWidth = expectedTileWorld;
    const int tx = (int)floorf((worldPos.x - m_cachedBMin[0]) / tileWidth);
    const int ty = (int)floorf((worldPos.z - m_cachedBMin[2]) / tileWidth);

    if (tx < 0 || ty < 0 || tx >= m_cachedTileWidthCount || ty >= m_cachedTileHeightCount)
    {
        printf("[NavMeshData] BuildTileAt: posicao (%.2f, %.2f, %.2f) fora do grid de tiles (%d x %d).\n",
               worldPos.x, worldPos.y, worldPos.z, m_cachedTileWidthCount, m_cachedTileHeightCount);
        return false;
    }

    LoggingRcContext ctx;
    NavmeshBuildInput buildInput{ctx, m_cachedVerts, m_cachedTris};
    buildInput.nverts = (int)(m_cachedVerts.size() / 3);
    buildInput.ntris = (int)(m_cachedTris.size() / 3);
    rcVcopy(buildInput.meshBMin, m_cachedBMin);
    rcVcopy(buildInput.meshBMax, m_cachedBMax);
    buildInput.baseCfg = m_cachedBaseCfg;

    bool built = false;
    bool empty = false;
    if (!BuildSingleTile(buildInput, m_cachedSettings, tx, ty, m_nav, built, empty))
        return false;

    outTileX = tx;
    outTileY = ty;
    if (empty)
    {
        printf("[NavMeshData] BuildTileAt: tile %d,%d nao possui geometrias para navmesh.\n", tx, ty);
    }
    else if (built)
    {
        printf("[NavMeshData] BuildTileAt: tile %d,%d reconstruido a partir do clique (%.2f, %.2f, %.2f).\n",
               tx, ty, worldPos.x, worldPos.y, worldPos.z);
    }
    return true;
}

bool NavMeshData::RemoveTileAt(const glm::vec3& worldPos,
                               int& outTileX,
                               int& outTileY)
{
    outTileX = -1;
    outTileY = -1;

    if (!m_nav)
    {
        printf("[NavMeshData] RemoveTileAt: navmesh ainda nao foi construido.\n");
        return false;
    }

    if (!m_hasTiledCache)
    {
        printf("[NavMeshData] RemoveTileAt: sem cache de build tiled. Gere o navmesh tiled primeiro.\n");
        return false;
    }

    const float tileWidth = m_cachedSettings.tileSize * m_cachedBaseCfg.cs;
    const int tx = (int)floorf((worldPos.x - m_cachedBMin[0]) / tileWidth);
    const int ty = (int)floorf((worldPos.z - m_cachedBMin[2]) / tileWidth);

    if (tx < 0 || ty < 0 || tx >= m_cachedTileWidthCount || ty >= m_cachedTileHeightCount)
    {
        printf("[NavMeshData] RemoveTileAt: posicao (%.2f, %.2f, %.2f) fora do grid de tiles (%d x %d).\n",
               worldPos.x, worldPos.y, worldPos.z, m_cachedTileWidthCount, m_cachedTileHeightCount);
        return false;
    }

    const dtTileRef tileRef = m_nav->getTileRefAt(tx, ty, 0);
    if (tileRef == 0)
    {
        printf("[NavMeshData] RemoveTileAt: tile %d,%d nao esta carregada.\n", tx, ty);
        return false;
    }

    unsigned char* tileData = nullptr;
    int tileDataSize = 0;
    const dtStatus status = m_nav->removeTile(tileRef, &tileData, &tileDataSize);
    if (dtStatusFailed(status))
    {
        printf("[NavMeshData] RemoveTileAt: falhou ao remover tile %d,%d.\n", tx, ty);
        return false;
    }

    if (tileData)
        dtFree(tileData);

    outTileX = tx;
    outTileY = ty;
    printf("[NavMeshData] RemoveTileAt: tile %d,%d removida (bytes=%d).\n", tx, ty, tileDataSize);
    return true;
}

bool NavMeshData::RebuildTilesInBounds(const glm::vec3& bmin,
                                       const glm::vec3& bmax,
                                       const NavmeshGenerationSettings& settings,
                                       bool onlyExistingTiles,
                                       std::vector<std::pair<int, int>>* outTiles)
{
    std::vector<std::pair<int, int>> tiles;
    if (!CollectTilesInBounds(bmin, bmax, onlyExistingTiles, tiles))
        return false;

    return RebuildSpecificTiles(tiles, settings, onlyExistingTiles, outTiles);
}

bool NavMeshData::CollectTilesInBounds(const glm::vec3& bmin,
                                       const glm::vec3& bmax,
                                       bool onlyExistingTiles,
                                       std::vector<std::pair<int, int>>& outTiles) const
{
    outTiles.clear();

    if (!m_nav)
    {
        printf("[NavMeshData] CollectTilesInBounds: navmesh ainda nao foi construido.\n");
        return false;
    }

    if (!m_hasTiledCache)
    {
        printf("[NavMeshData] CollectTilesInBounds: sem cache de build tiled. Gere o navmesh tiled primeiro.\n");
        return false;
    }

    const float tileWidth = m_cachedSettings.tileSize * m_cachedBaseCfg.cs;
    int minTx = (int)floorf((bmin.x - m_cachedBMin[0]) / tileWidth);
    int minTy = (int)floorf((bmin.z - m_cachedBMin[2]) / tileWidth);
    int maxTx = (int)floorf((bmax.x - m_cachedBMin[0]) / tileWidth);
    int maxTy = (int)floorf((bmax.z - m_cachedBMin[2]) / tileWidth);

    minTx = std::max(0, minTx);
    minTy = std::max(0, minTy);
    maxTx = std::min(m_cachedTileWidthCount - 1, maxTx);
    maxTy = std::min(m_cachedTileHeightCount - 1, maxTy);

    if (minTx > maxTx || minTy > maxTy)
    {
        printf("[NavMeshData] CollectTilesInBounds: bounds fora do grid. BMin=(%.2f, %.2f, %.2f) BMax=(%.2f, %.2f, %.2f)\n",
               bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
        return false;
    }

    for (int ty = minTy; ty <= maxTy; ++ty)
    {
        for (int tx = minTx; tx <= maxTx; ++tx)
        {
            if (onlyExistingTiles && m_nav->getTileRefAt(tx, ty, 0) == 0)
                continue;
            outTiles.emplace_back(tx, ty);
        }
    }

    if (outTiles.empty())
    {
        printf("[NavMeshData] CollectTilesInBounds: nenhuma tile candidata no intervalo [%d,%d]-[%d,%d].\n",
               minTx, minTy, maxTx, maxTy);
    }

    return true;
}

bool NavMeshData::RebuildSpecificTiles(const std::vector<std::pair<int, int>>& tiles,
                                       const NavmeshGenerationSettings& settings,
                                       bool onlyExistingTiles,
                                       std::vector<std::pair<int, int>>* outTiles)
{
    if (outTiles)
        outTiles->clear();

    if (!m_nav)
    {
        printf("[NavMeshData] RebuildSpecificTiles: navmesh ainda nao foi construido.\n");
        return false;
    }

    if (!m_hasTiledCache)
    {
        printf("[NavMeshData] RebuildSpecificTiles: sem cache de build tiled. Gere o navmesh tiled primeiro.\n");
        return false;
    }

    if (settings.mode != NavmeshBuildMode::Tiled ||
        fabsf(settings.cellSize - m_cachedSettings.cellSize) > 1e-3f ||
        fabsf(settings.cellHeight - m_cachedSettings.cellHeight) > 1e-3f ||
        settings.tileSize != m_cachedSettings.tileSize)
    {
        printf("[NavMeshData] RebuildSpecificTiles: configuracao atual difere da usada no navmesh cacheado. Use mesmos valores de tile/cell.\n");
    }

    LoggingRcContext ctx;
    NavmeshBuildInput buildInput{ctx, m_cachedVerts, m_cachedTris};
    buildInput.nverts = (int)(m_cachedVerts.size() / 3);
    buildInput.ntris = (int)(m_cachedTris.size() / 3);
    rcVcopy(buildInput.meshBMin, m_cachedBMin);
    rcVcopy(buildInput.meshBMax, m_cachedBMax);
    buildInput.baseCfg = m_cachedBaseCfg;

    bool anyTouched = false;

    for (const auto& tile : tiles)
    {
        const int tx = tile.first;
        const int ty = tile.second;
        if (tx < 0 || ty < 0 || tx >= m_cachedTileWidthCount || ty >= m_cachedTileHeightCount)
        {
            printf("[NavMeshData] RebuildSpecificTiles: tile %d,%d fora do grid (%d x %d).\n", tx, ty, m_cachedTileWidthCount, m_cachedTileHeightCount);
            continue;
        }

        if (onlyExistingTiles && m_nav->getTileRefAt(tx, ty, 0) == 0)
            continue;

        bool built = false;
        bool empty = false;
        if (!BuildSingleTile(buildInput, m_cachedSettings, tx, ty, m_nav, built, empty))
        {
            printf("[NavMeshData] RebuildSpecificTiles: falhou ao reconstruir tile %d,%d.\n", tx, ty);
            return false;
        }

        if (built || empty)
        {
            anyTouched = true;
            if (outTiles)
                outTiles->emplace_back(tx, ty);
        }
    }

    if (!anyTouched)
    {
        printf("[NavMeshData] RebuildSpecificTiles: nenhuma tile elegivel no conjunto fornecido (%zu entradas).\n", tiles.size());
    }

    return true;
}

bool NavMeshData::UpdateCachedGeometry(const std::vector<glm::vec3>& verts,
                                       const std::vector<unsigned int>& indices)
{
    if (!m_nav || !m_hasTiledCache)
    {
        printf("[NavMeshData] UpdateCachedGeometry: navmesh tiled nao inicializado.\n");
        return false;
    }

    const size_t nverts = verts.size();
    const size_t ntris = indices.size() / 3;
    if (nverts == 0 || ntris == 0)
    {
        printf("[NavMeshData] UpdateCachedGeometry: geometria vazia.\n");
        return false;
    }

    std::vector<float> convertedVerts(nverts * 3);
    for (size_t i = 0; i < nverts; ++i)
    {
        convertedVerts[i * 3 + 0] = verts[i].x;
        convertedVerts[i * 3 + 1] = verts[i].y;
        convertedVerts[i * 3 + 2] = verts[i].z;
    }

    std::vector<int> convertedTris(indices.size());
    for (size_t i = 0; i < indices.size(); ++i)
    {
        convertedTris[i] = static_cast<int>(indices[i]);
    }

    m_cachedVerts = std::move(convertedVerts);
    m_cachedTris = std::move(convertedTris);
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
                                const NavmeshGenerationSettings& settings,
                                bool buildTilesNow)
{
    m_hasTiledCache = false;
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

    if (m_nav)
    {
        dtFreeNavMesh(m_nav);
        m_nav = nullptr;
    }

    const int nverts = (int)vertsIn.size();
    const int ntris  = (int)(idxIn.size() / 3);

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

    NavmeshBuildInput buildInput{ctx, verts, tris, nverts, ntris};
    rcVcopy(buildInput.meshBMin, meshBMin);
    rcVcopy(buildInput.meshBMax, meshBMax);
    buildInput.baseCfg = baseCfg;

    dtNavMesh* newNav = nullptr;
    bool ok = false;
    if (settings.mode == NavmeshBuildMode::SingleMesh)
    {
        ok = BuildSingleNavMesh(buildInput, settings, newNav);
    }
    else
    {
        ok = BuildTiledNavMesh(buildInput, settings, newNav, buildTilesNow, nullptr, nullptr);
    }

    if (!ok)
    {
        if (newNav)
        {
            dtFreeNavMesh(newNav);
        }
        return false;
    }

    m_nav = newNav;
    if (settings.mode == NavmeshBuildMode::Tiled)
    {
        m_cachedVerts = verts;
        m_cachedTris = tris;
        rcVcopy(m_cachedBMin, meshBMin);
        rcVcopy(m_cachedBMax, meshBMax);
        m_cachedBaseCfg = baseCfg;
        m_cachedSettings = settings;
        m_cachedTileWidthCount = (baseCfg.width + settings.tileSize - 1) / settings.tileSize;
        m_cachedTileHeightCount = (baseCfg.height + settings.tileSize - 1) / settings.tileSize;
        m_hasTiledCache = true;
    }
    else
    {
        m_cachedVerts.clear();
        m_cachedTris.clear();
        m_hasTiledCache = false;
    }
    return true;
}
