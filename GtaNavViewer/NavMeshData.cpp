#include "NavMeshData.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourMath.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>

#include "NavMeshBuild.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include <tuple>
#include <unordered_set>
#include <functional>

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

    glm::vec3 GetPolyVertex(const dtMeshTile* tile, const dtPoly* poly, int index)
    {
        const int vertIndex = poly->verts[index];
        const float* v = &tile->verts[vertIndex * 3];
        return glm::vec3(v[0], v[1], v[2]);
    }

    glm::vec3 ComputePolyNormal(const dtMeshTile* tile, const dtPoly* poly)
    {
        if (!tile || !poly || poly->vertCount < 3)
            return glm::vec3(0.0f, 1.0f, 0.0f);

        const glm::vec3 a = GetPolyVertex(tile, poly, 0);
        const glm::vec3 b = GetPolyVertex(tile, poly, 1);
        const glm::vec3 c = GetPolyVertex(tile, poly, 2);
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z))
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        return normal;
    }

    glm::vec3 ComputePolyCentroid(const dtMeshTile* tile, const dtPoly* poly)
    {
        glm::vec3 sum(0.0f);
        for (int i = 0; i < poly->vertCount; ++i)
            sum += GetPolyVertex(tile, poly, i);
        if (poly->vertCount > 0)
            sum /= static_cast<float>(poly->vertCount);
        return sum;
    }

    glm::vec3 ComputeEdgeOutwardNormal(const glm::vec3& a,
                                       const glm::vec3& b,
                                       const glm::vec3& polyCenter)
    {
        const glm::vec3 edgeDir = glm::normalize(b - a);
        glm::vec3 outward = glm::normalize(glm::vec3(-edgeDir.z, 0.0f, edgeDir.x));
        const glm::vec3 toEdge = glm::normalize(glm::vec3((a.x + b.x) * 0.5f - polyCenter.x,
                                                          0.0f,
                                                          (a.z + b.z) * 0.5f - polyCenter.z));
        if (glm::dot(outward, toEdge) < 0.0f)
            outward = -outward;
        return outward;
    }

    bool IntersectSegmentTriangle(const glm::vec3& sp,
                                  const glm::vec3& sq,
                                  const glm::vec3& a,
                                  const glm::vec3& b,
                                  const glm::vec3& c,
                                  float& tOut,
                                  glm::vec3& normalOut)
    {
        glm::vec3 ab = b - a;
        glm::vec3 ac = c - a;
        glm::vec3 qp = sp - sq;

        glm::vec3 norm = glm::cross(ab, ac);
        float d = glm::dot(qp, norm);
        if (d <= 0.0f)
            return false;

        glm::vec3 ap = sp - a;
        float t = glm::dot(ap, norm);
        if (t < 0.0f || t > d)
            return false;

        glm::vec3 e = glm::cross(qp, ap);
        float v = glm::dot(ac, e);
        if (v < 0.0f || v > d)
            return false;
        float w = -glm::dot(ab, e);
        if (w < 0.0f || v + w > d)
            return false;

        tOut = t / d;
        normalOut = norm;
        return true;
    }

    bool RaycastDown(const std::vector<float>& verts,
                     const std::vector<int>& tris,
                     const glm::vec3& start,
                     float maxDistance,
                     glm::vec3& outHit,
                     glm::vec3& outNormal)
    {
        if (verts.empty() || tris.empty())
            return false;

        const glm::vec3 end = start - glm::vec3(0.0f, maxDistance, 0.0f);
        float closestT = 1.0f;
        bool hit = false;

        for (size_t i = 0; i + 2 < tris.size(); i += 3)
        {
            const int ia = tris[i];
            const int ib = tris[i + 1];
            const int ic = tris[i + 2];
            if ((ia < 0 || ib < 0 || ic < 0) ||
                (static_cast<size_t>(ia * 3 + 2) >= verts.size()) ||
                (static_cast<size_t>(ib * 3 + 2) >= verts.size()) ||
                (static_cast<size_t>(ic * 3 + 2) >= verts.size()))
                continue;

            const glm::vec3 a(verts[ia * 3 + 0], verts[ia * 3 + 1], verts[ia * 3 + 2]);
            const glm::vec3 b(verts[ib * 3 + 0], verts[ib * 3 + 1], verts[ib * 3 + 2]);
            const glm::vec3 c(verts[ic * 3 + 0], verts[ic * 3 + 1], verts[ic * 3 + 2]);

            float t = 1.0f;
            glm::vec3 triNormal;
            if (IntersectSegmentTriangle(start, end, a, b, c, t, triNormal))
            {
                if (t < closestT)
                {
                    closestT = t;
                    hit = true;
                    outHit = start + (end - start) * t;
                    outNormal = triNormal;
                }
            }
        }

        if (hit)
        {
            const float nlen = glm::length(outNormal);
            if (nlen > 1e-4f)
                outNormal /= nlen;
            else
                outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        return hit;
    }

    bool RaycastTo(const std::vector<float>& verts,
                     const std::vector<int>& tris,
                     const glm::vec3& start,
                     const glm::vec3& end,
                     glm::vec3& outHit,
                     glm::vec3& outNormal)
    {
        if (verts.empty() || tris.empty())
            return false;

        float closestT = 1.0f;
        bool hit = false;

        for (size_t i = 0; i + 2 < tris.size(); i += 3)
        {
            const int ia = tris[i];
            const int ib = tris[i + 1];
            const int ic = tris[i + 2];
            if ((ia < 0 || ib < 0 || ic < 0) ||
                (static_cast<size_t>(ia * 3 + 2) >= verts.size()) ||
                (static_cast<size_t>(ib * 3 + 2) >= verts.size()) ||
                (static_cast<size_t>(ic * 3 + 2) >= verts.size()))
                continue;

            const glm::vec3 a(verts[ia * 3 + 0], verts[ia * 3 + 1], verts[ia * 3 + 2]);
            const glm::vec3 b(verts[ib * 3 + 0], verts[ib * 3 + 1], verts[ib * 3 + 2]);
            const glm::vec3 c(verts[ic * 3 + 0], verts[ic * 3 + 1], verts[ic * 3 + 2]);

            float t = 1.0f;
            glm::vec3 triNormal;
            if (IntersectSegmentTriangle(start, end, a, b, c, t, triNormal))
            {
                if (t < closestT)
                {
                    closestT = t;
                    hit = true;
                    outHit = start + (end - start) * t;
                    outNormal = triNormal;
                }
            }
        }

        if (hit)
        {
            const float nlen = glm::length(outNormal);
            if (nlen > 1e-4f)
                outNormal /= nlen;
            else
                outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        return hit;
    }

    dtPolyRef GetNeighbourRef(const dtMeshTile* tile,
                              const dtPoly* poly,
                              int edge,
                              const dtNavMesh* nav)
    {
        const unsigned short nei = poly->neis[edge];
        if (nei == 0)
            return 0;

        if ((nei & DT_EXT_LINK) == 0)
        {
            const unsigned int neighbourIndex = static_cast<unsigned int>(nei - 1);
            return nav->getPolyRefBase(tile) | neighbourIndex;
        }

        for (unsigned int linkIndex = poly->firstLink; linkIndex != DT_NULL_LINK; linkIndex = tile->links[linkIndex].next)
        {
            const dtLink& link = tile->links[linkIndex];
            if (link.edge == edge)
                return link.ref;
        }

        return 0;
    }

    struct Tuple3Hash
    {
        size_t operator()(const std::tuple<int, int, int>& v) const noexcept
        {
            const auto [x, y, z] = v;
            size_t seed = 0;
            seed ^= std::hash<int>{}(x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<int>{}(y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<int>{}(z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::tuple<int, int, int> QuantizePosition(const glm::vec3& p, float scale)
    {
        return {
            static_cast<int>(std::round(p.x * scale)),
            static_cast<int>(std::round(p.y * scale)),
            static_cast<int>(std::round(p.z * scale))
        };
    }
}

NavMeshData::~NavMeshData()
{
    if (m_nav)
    {
        dtFreeNavMesh(m_nav);
        m_nav = nullptr;
    }
}

void NavMeshData::AddOffmeshLink(const glm::vec3& start,
                                 const glm::vec3& end,
                                 float radius,
                                 bool bidirectional)
{
    OffmeshLink link{};
    link.start = start;
    link.end = end;
    link.radius = radius;
    link.bidirectional = bidirectional;
    link.area = RC_WALKABLE_AREA;
    link.flags = 1;
    m_offmeshLinks.push_back(link);
}

bool NavMeshData::RemoveNearestOffmeshLink(const glm::vec3& point)
{
    if (m_offmeshLinks.empty())
        return false;

    const auto pointToSegmentDistSq = [](const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
    {
        const glm::vec3 ab = b - a;
        const float denom = glm::dot(ab, ab);
        if (denom <= 0.0f)
            return glm::dot(p - a, p - a);

        float t = glm::dot(p - a, ab) / denom;
        t = glm::clamp(t, 0.0f, 1.0f);
        const glm::vec3 closest = a + ab * t;
        return glm::dot(p - closest, p - closest);
    };

    size_t nearestIndex = 0;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_offmeshLinks.size(); ++i)
    {
        const auto& link = m_offmeshLinks[i];
        float distSq = pointToSegmentDistSq(point, link.start, link.end);
        if (distSq < nearestDistSq)
        {
            nearestDistSq = distSq;
            nearestIndex = i;
        }
    }

    m_offmeshLinks.erase(m_offmeshLinks.begin() + static_cast<std::ptrdiff_t>(nearestIndex));
    return true;
}

void NavMeshData::SetOffmeshLinks(std::vector<OffmeshLink> links)
{
    m_offmeshLinks = std::move(links);
}

void NavMeshData::ClearOffmeshLinks()
{
    m_offmeshLinks.clear();
}

bool NavMeshData::GenerateAutomaticOffmeshLinks(const AutoOffmeshGenerationParams& params,
                                                std::vector<OffmeshLink>& outLinks) const
{
    outLinks.clear();

    if (!m_nav)
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: navmesh nao inicializado.\n");
        return false;
    }

    if (params.jumpHeight < 0.0f || params.maxDropHeight <= 0.0f)
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: parametros invalidos (jumpHeight=%.2f, maxDrop=%.2f).\n",
               params.jumpHeight, params.maxDropHeight);
        return false;
    }

    if (m_cachedVerts.empty() || m_cachedTris.empty())
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: sem geometria cacheada para raycast.\n");
        return false;
    }

    const float slopeCos = cosf(glm::radians(params.maxSlopeDegrees));
    const float maxRayDistance = params.maxDropHeight + params.raycastExtraHeight + params.upOffset;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const float startInset = 0.04f; // Empurra o start para dentro do polígono (~4cm)

    const int maxTiles = m_nav->getMaxTiles();
    size_t reserved = static_cast<size_t>(maxTiles) * 4;
    outLinks.reserve(reserved);

    std::unordered_set<std::tuple<int, int, int>, Tuple3Hash> usedStartHashes;
    usedStartHashes.reserve(reserved);

    for (int tileIndex = 0; tileIndex < maxTiles; ++tileIndex)
    {
        const dtMeshTile* tile = m_nav->getTile(tileIndex);
        if (!tile || !tile->header)
            continue;

        for (int polyIndex = 0; polyIndex < tile->header->polyCount; ++polyIndex)
        {
            const dtPoly* poly = &tile->polys[polyIndex];
            if (poly->getType() != DT_POLYTYPE_GROUND)
                continue;

            const glm::vec3 polyNormal = ComputePolyNormal(tile, poly);
            if (polyNormal.y < slopeCos)
                continue;

            const glm::vec3 polyCenter = ComputePolyCentroid(tile, poly);
            const int vertCount = poly->vertCount;
            for (int edge = 0; edge < vertCount; ++edge)
            {
                const int vaIndex = edge;
                const int vbIndex = (edge + 1) % vertCount;
                const glm::vec3 va = GetPolyVertex(tile, poly, vaIndex);
                const glm::vec3 vb = GetPolyVertex(tile, poly, vbIndex);

                const dtPolyRef neighbourRef = GetNeighbourRef(tile, poly, edge, m_nav);
                if (neighbourRef != 0)
                {
                    const dtMeshTile* neiTile = nullptr;
                    const dtPoly* neiPoly = nullptr;
                    m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neiTile, &neiPoly);
                    if (!neiTile || !neiPoly || neiPoly->getType() != DT_POLYTYPE_GROUND)
                        continue;

                    const float heightDelta = polyCenter.y - ComputePolyCentroid(neiTile, neiPoly).y;
                    if (heightDelta < params.minNeighborHeightDelta)
                        continue;
                }

                const glm::vec3 mid = (va + vb) * 0.5f;
                glm::vec3 outward = ComputeEdgeOutwardNormal(va, vb, polyCenter);
                if (!std::isfinite(outward.x) || !std::isfinite(outward.z))
                    continue;

                const glm::vec3 testPoint = mid + outward * params.edgeOutset + up * params.upOffset;

                glm::vec3 hitPoint;
                glm::vec3 hitNormal;
                if (!RaycastDown(m_cachedVerts, m_cachedTris, testPoint, maxRayDistance, hitPoint, hitNormal))
                    continue;

                glm::vec3 hitPoint2;
                glm::vec3 hitNormal2;
                if (RaycastTo(m_cachedVerts, m_cachedTris, hitPoint, testPoint, hitPoint2, hitNormal2))
                    continue;

                const float drop = testPoint.y - hitPoint.y;
                if (drop < params.minDropThreshold || drop > params.maxDropHeight)
                    continue;

                if (glm::dot(hitNormal, up) < slopeCos)
                    continue;

                const auto hash = QuantizePosition(mid, 10.0f);
                if (usedStartHashes.find(hash) != usedStartHashes.end())
                    continue;
                usedStartHashes.insert(hash);

                OffmeshLink link{};
                link.start = mid - outward * startInset;
                link.end = hitPoint;
                link.radius = params.agentRadius;
                link.bidirectional = drop <= params.jumpHeight;
                link.area = params.dropArea;
                link.flags = 1;
                link.userId = params.userIdBase + static_cast<unsigned int>(outLinks.size());
                outLinks.push_back(link);
            }
        }
    }

    printf("[NavMeshData] GenerateAutomaticOffmeshLinks: gerados %zu links automaticos.\n", outLinks.size());
    return true;
}
NavMeshData::NavMeshData(NavMeshData&& other) noexcept
{
    *this = std::move(other);
}

NavMeshData& NavMeshData::operator=(NavMeshData&& other) noexcept
{
    if (this != &other)
    {
        if (m_nav)
        {
            dtFreeNavMesh(m_nav);
        }

        m_nav = other.m_nav;
        other.m_nav = nullptr;

        m_cachedVerts = std::move(other.m_cachedVerts);
        m_cachedTris = std::move(other.m_cachedTris);
        std::memcpy(m_cachedBMin, other.m_cachedBMin, sizeof(m_cachedBMin));
        std::memcpy(m_cachedBMax, other.m_cachedBMax, sizeof(m_cachedBMax));
        m_cachedBaseCfg = other.m_cachedBaseCfg;
        m_cachedSettings = other.m_cachedSettings;
        m_cachedTileWidthCount = other.m_cachedTileWidthCount;
        m_cachedTileHeightCount = other.m_cachedTileHeightCount;
        m_hasTiledCache = other.m_hasTiledCache;
        m_offmeshLinks = std::move(other.m_offmeshLinks);

        std::memset(other.m_cachedBMin, 0, sizeof(other.m_cachedBMin));
        std::memset(other.m_cachedBMax, 0, sizeof(other.m_cachedBMax));
        other.m_cachedTileWidthCount = 0;
        other.m_cachedTileHeightCount = 0;
        other.m_hasTiledCache = false;
    }
    return *this;
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
    buildInput.offmeshLinks = &m_offmeshLinks;

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
    buildInput.offmeshLinks = &m_offmeshLinks;

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
                                bool buildTilesNow,
                                const std::atomic_bool* cancelFlag,
                                bool useCache,
                                const char* cachePath)
{
    m_hasTiledCache = false;
    LoggingRcContext ctx;

    auto isCancelled = [cancelFlag]()
    {
        return cancelFlag && cancelFlag->load();
    };

    if (isCancelled())
    {
        printf("[NavMeshData] BuildFromMesh abortado antes de iniciar (cancelado).\n");
        return false;
    }

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
        if (isCancelled())
        {
            printf("[NavMeshData] BuildFromMesh abortado durante validacao de indices.\n");
            return false;
        }
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
        if (isCancelled())
        {
            printf("[NavMeshData] BuildFromMesh abortado durante copia de vertices.\n");
            return false;
        }
        verts[i*3+0] = vertsIn[i].x;
        verts[i*3+1] = vertsIn[i].y;
        verts[i*3+2] = vertsIn[i].z;
    }

    std::vector<int> tris(ntris * 3);
    for (int i = 0; i < ntris * 3; ++i)
        tris[i] = (int)idxIn[i];

    if (isCancelled())
    {
        printf("[NavMeshData] BuildFromMesh abortado apos copia de indices.\n");
        return false;
    }

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
    buildInput.offmeshLinks = &m_offmeshLinks;

    dtNavMesh* newNav = nullptr;
    bool ok = false;
    if (settings.mode == NavmeshBuildMode::SingleMesh)
    {
        ok = BuildSingleNavMesh(buildInput, settings, newNav);
    }
    else
    {
        ok = BuildTiledNavMesh(buildInput, settings, newNav, buildTilesNow, nullptr, nullptr, cancelFlag, useCache, cachePath);
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
    m_cachedVerts = verts;
    m_cachedTris = tris;
    rcVcopy(m_cachedBMin, meshBMin);
    rcVcopy(m_cachedBMax, meshBMax);
    m_cachedBaseCfg = baseCfg;
    m_cachedSettings = settings;
    if (settings.mode == NavmeshBuildMode::Tiled)
    {
        m_cachedTileWidthCount = (baseCfg.width + settings.tileSize - 1) / settings.tileSize;
        m_cachedTileHeightCount = (baseCfg.height + settings.tileSize - 1) / settings.tileSize;
        m_hasTiledCache = true;
    }
    else
    {
        m_cachedTileWidthCount = 0;
        m_cachedTileHeightCount = 0;
        m_hasTiledCache = false;
    }
    return true;
}
