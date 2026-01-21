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
#include <unordered_map>
#include <cstdint>
#include <memory>

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

    void FillBaseConfig(const NavmeshGenerationSettings& settings, rcConfig& cfg)
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
    }

    bool ComputeTiledGridCounts(const NavmeshGenerationSettings& settings,
                                const float* bmin,
                                const float* bmax,
                                TileGridStats& outStats)
    {
        if (!bmin || !bmax)
            return false;

        const float boundsWidth = std::max(0.0f, bmax[0] - bmin[0]);
        const float boundsHeight = std::max(0.0f, bmax[2] - bmin[2]);
        const float tileWorld = settings.tileSize * settings.cellSize;
        if (tileWorld <= 0.0f)
            return false;

        const int tilesX = std::max(1, static_cast<int>(std::ceil(boundsWidth / tileWorld)));
        const int tilesY = std::max(1, static_cast<int>(std::ceil(boundsHeight / tileWorld)));

        outStats.tileWorld = tileWorld;
        outStats.boundsWidth = boundsWidth;
        outStats.boundsHeight = boundsHeight;
        outStats.tileCountX = tilesX;
        outStats.tileCountY = tilesY;
        outStats.tileCountTotal = tilesX * tilesY;
        return true;
    }

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

    struct OrientedRectXZ
    {
        glm::vec3 center{0.0f};
        glm::vec3 extents{0.0f};
        float sinAngle = 0.0f;
        float cosAngle = 1.0f;

        bool Contains(const glm::vec3& p) const
        {
            const float dx = p.x - center.x;
            const float dz = p.z - center.z;
            const float localX = cosAngle * dx + sinAngle * dz;
            const float localZ = -sinAngle * dx + cosAngle * dz;
            return std::fabs(localX) <= extents.x && std::fabs(localZ) <= extents.z;
        }

        void ComputeAabb(float outBMin[3], float outBMax[3]) const
        {
            const float halfX = std::fabs(cosAngle) * extents.x + std::fabs(sinAngle) * extents.z;
            const float halfZ = std::fabs(sinAngle) * extents.x + std::fabs(cosAngle) * extents.z;
            outBMin[0] = center.x - halfX;
            outBMin[1] = center.y - extents.y;
            outBMin[2] = center.z - halfZ;

            outBMax[0] = center.x + halfX;
            outBMax[1] = center.y + extents.y;
            outBMax[2] = center.z + halfZ;
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
    link.area = AREA_OFFMESH;
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

    const bool generateDropJump = (params.linksGenFlags & 1) != 0;
    const bool generateFacingNormals = (params.linksGenFlags & 2) != 0;

    if (!m_nav)
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: navmesh nao inicializado.\n");
        return false;
    }

    if (!generateDropJump && !generateFacingNormals)
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: nenhum modo solicitado (linksGenFlags=0).\n");
        return true;
    }

    if (generateDropJump && (params.jumpHeight < 0.0f || params.maxDropHeight <= 0.0f))
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: parametros invalidos (jumpHeight=%.2f, maxDrop=%.2f).\n",
               params.jumpHeight, params.maxDropHeight);
        return false;
    }

    if (generateFacingNormals)
    {
        if (params.angleTolerance < 0.0f || params.angleTolerance > 180.0f ||
            params.maxHeightDiff <= 0.0f ||
            params.maxDistance <= 0.0f ||
            params.minDistance < 0.0f ||
            params.minDistance > params.maxDistance)
        {
            printf("[NavMeshData] GenerateAutomaticOffmeshLinks: parametros de facing invalidos (angleTol=%.2f, maxHeightDiff=%.2f, minDist=%.2f, maxDist=%.2f).\n",
                   params.angleTolerance, params.maxHeightDiff, params.minDistance, params.maxDistance);
            return false;
        }
    }

    if (m_cachedVerts.empty() || m_cachedTris.empty())
    {
        printf("[NavMeshData] GenerateAutomaticOffmeshLinks: sem geometria cacheada para raycast.\n");
        return false;
    }

    const float slopeCos = cosf(glm::radians(params.maxSlopeDegrees));
    const float maxRayDistance = generateDropJump ? (params.maxDropHeight + params.raycastExtraHeight + params.upOffset) : 0.0f;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const float startInset = 0.04f; // Empurra o start para dentro do polígono (~4cm)

    const int maxTiles = m_nav->getMaxTiles();
    size_t reserved = static_cast<size_t>(maxTiles) * 4;
    outLinks.reserve(reserved);

    std::unordered_set<std::tuple<int, int, int>, Tuple3Hash> usedStartHashes;
    if (generateDropJump)
        usedStartHashes.reserve(reserved);

    struct EdgeCandidate
    {
        glm::vec3 mid;
        glm::vec3 outward;
        int tileX = -1;
        int tileY = -1;
    };

    std::vector<EdgeCandidate> facingEdges;
    facingEdges.reserve(reserved);

    const auto resolveTileCoords = [&](const dtMeshTile* tile, const glm::vec3& pos) -> std::pair<int, int>
    {
        if (tile && tile->header)
            return {tile->header->x, tile->header->y};

        const float tileWidth = m_cachedSettings.tileSize * m_cachedBaseCfg.cs;
        if (m_hasTiledCache && tileWidth > 0.0f)
        {
            const int tx = (int)floorf((pos.x - m_gridBMin[0]) / tileWidth);
            const int ty = (int)floorf((pos.z - m_gridBMin[2]) / tileWidth);
            return {tx, ty};
        }

        return {-1, -1};
    };

    size_t dropGenerated = 0;

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
                const bool hasNeighbour = neighbourRef != 0;

                glm::vec3 mid(0.0f);
                glm::vec3 outward(0.0f);
                bool outwardValid = false;
                if (generateDropJump || (generateFacingNormals && !hasNeighbour))
                {
                    mid = (va + vb) * 0.5f;
                    outward = ComputeEdgeOutwardNormal(va, vb, polyCenter);
                    outwardValid = std::isfinite(outward.x) && std::isfinite(outward.z);
                }

                if (generateDropJump)
                {
                    if (hasNeighbour)
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

                    if (!outwardValid)
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
                    link.radius = 1.0;
                    link.bidirectional = false;
                    link.area = AREA_DROP;
                    link.flags = 1;
                    link.userId = params.userIdBase + static_cast<unsigned int>(outLinks.size());
                    const auto [tx, ty] = resolveTileCoords(tile, link.start);
                    link.ownerTx = tx;
                    link.ownerTy = ty;
                    outLinks.push_back(link);
                    ++dropGenerated;
                    if (drop <= params.jumpHeight) {
                        OffmeshLink link2{};
                        link2.start = hitPoint;
                        link2.end = mid - outward * startInset;
                        link2.radius = 1.0;
                        link2.bidirectional = false;
                        link2.area = AREA_JUMP;
                        link2.flags = 1;
                        link2.userId = params.userIdBase + static_cast<unsigned int>(outLinks.size());
                        const auto [tx2, ty2] = resolveTileCoords(tile, link2.start);
                        link2.ownerTx = tx2;
                        link2.ownerTy = ty2;
                        outLinks.push_back(link2);
                        ++dropGenerated;
                    }
                }

                if (generateFacingNormals && !hasNeighbour && outwardValid)
                {
                    const auto [tx, ty] = resolveTileCoords(tile, mid);
                    facingEdges.push_back({mid, outward, tx, ty});
                }
            }
        }
    }

    size_t facingGenerated = 0;
    size_t rejectedAngle = 0;
    size_t rejectedHeight = 0;
    size_t rejectedDistance = 0;
    size_t rejectedObstruction = 0;

    if (generateFacingNormals && !facingEdges.empty())
    {
        std::unordered_map<int64_t, std::pair<size_t, size_t>> tileCounters;
        tileCounters.reserve(facingEdges.size());
        const float facingTarget = 180.0f;

        for (size_t i = 0; i + 1 < facingEdges.size(); ++i)
        {
            const auto& edgeA = facingEdges[i];
            glm::vec2 normA(edgeA.outward.x, edgeA.outward.z);
            const float lenA = glm::length(normA);
            if (lenA < 1e-3f)
                continue;
            normA /= lenA;
            const int64_t tileKey = (static_cast<int64_t>(edgeA.tileX) << 32) | (static_cast<int64_t>(edgeA.tileY) & 0xffffffff);
            auto& counters = tileCounters[tileKey];

            for (size_t j = i + 1; j < facingEdges.size(); ++j)
            {
                const auto& edgeB = facingEdges[j];
                glm::vec2 normB(edgeB.outward.x, edgeB.outward.z);
                const float lenB = glm::length(normB);
                if (lenB < 1e-3f)
                    continue;
                normB /= lenB;

                const float dot = glm::clamp(glm::dot(normA, normB), -1.0f, 1.0f);
                const float angleDeg = glm::degrees(acosf(dot));
                if (fabsf(facingTarget - angleDeg) > params.angleTolerance)
                {
                    ++rejectedAngle;
                    ++counters.second;
                    continue;
                }

                const float heightDiff = fabsf(edgeA.mid.y - edgeB.mid.y);
                if (heightDiff > params.maxHeightDiff || heightDiff < params.minHeightDiff)
                {
                    ++rejectedHeight;
                    ++counters.second;
                    continue;
                }

                const float distance = glm::length(edgeB.mid - edgeA.mid);
                if (distance < params.minDistance || distance > params.maxDistance)
                {
                    ++rejectedDistance;
                    ++counters.second;
                    continue;
                }

                const glm::vec3 offsetA = edgeA.mid + edgeA.outward * params.normalOffset + up * params.zOffset;
                const glm::vec3 offsetB = edgeB.mid + edgeB.outward * params.normalOffset + up * params.zOffset;

                glm::vec3 obstructionHit;
                glm::vec3 obstructionNormal;
                if (RaycastTo(m_cachedVerts, m_cachedTris, offsetA, offsetB, obstructionHit, obstructionNormal))
                {
                    ++rejectedObstruction;
                    ++counters.second;
                    continue;
                }

                OffmeshLink link{};
                link.start = edgeA.mid;
                link.end = edgeB.mid;
                link.radius = 1.0;
                link.bidirectional = true;
                link.area = AREA_OFFMESH;
                link.flags = 1;
                link.userId = params.userIdBase + static_cast<unsigned int>(outLinks.size());
                link.ownerTx = edgeA.tileX;
                link.ownerTy = edgeA.tileY;
                outLinks.push_back(link);
                ++facingGenerated;
                ++counters.first;
            }
        }

        for (auto& kv : tileCounters)
        {
            const int tx = static_cast<int>(kv.first >> 32);
            const int ty = static_cast<int>(kv.first & 0xffffffff);
            printf("[AutoOffmesh] Tile (%d,%d): accepted=%zu rejected=%zu\n", tx, ty, kv.second.first, kv.second.second);
        }
    }

    printf("[AutoOffmesh] drop/jump links gerados: %zu\n", dropGenerated);
    printf("[AutoOffmesh] facing-normal links gerados: %zu\n", facingGenerated);
    if (generateFacingNormals)
    {
        printf("[AutoOffmesh] facing-normal rejeitados (angulo=%zu, altura=%zu, distancia=%zu, obstrucao=%zu)\n",
               rejectedAngle, rejectedHeight, rejectedDistance, rejectedObstruction);
    }

    printf("[NavMeshData] GenerateAutomaticOffmeshLinks: gerados %zu links automaticos.\n", outLinks.size());
    return true;
}

bool NavMeshData::AddOffmeshLinksToNavMeshIsland(const IslandOffmeshLinkParams& params,
                                                 std::vector<OffmeshLink>& outLinks)
{
    outLinks.clear();

    if (!m_nav)
    {
        printf("[NavMeshData] AddOffmeshLinksToNavMeshIsland: navmesh nao inicializado.\n");
        return false;
    }

    if (m_cachedVerts.empty() || m_cachedTris.empty())
    {
        printf("[NavMeshData] AddOffmeshLinksToNavMeshIsland: sem geometria cacheada para raycast.\n");
        return false;
    }

    if (params.maxDistance <= 0.0f || params.minDistance < 0.0f || params.minDistance > params.maxDistance || params.maxHeightDiff < 0.0f)
    {
        printf("[NavMeshData] AddOffmeshLinksToNavMeshIsland: parametros invalidos (minDist=%.2f, maxDist=%.2f, maxHeightDiff=%.2f).\n",
               params.minDistance, params.maxDistance, params.maxHeightDiff);
        return false;
    }

    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query)
        return false;
    const std::unique_ptr<dtNavMeshQuery, void(*)(dtNavMeshQuery*)> queryGuard(query, dtFreeNavMeshQuery);

    if (dtStatusFailed(query->init(m_nav, 2048)))
        return false;

    dtQueryFilter filter{};
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    const float searchExtents[3] = {
        std::max(params.searchExtents.x, 0.1f),
        std::max(params.searchExtents.y, 0.1f),
        std::max(params.searchExtents.z, 0.1f)
    };

    const float targetPos[3] = { params.targetPosition.x, params.targetPosition.y, params.targetPosition.z };
    dtPolyRef targetRef = 0;
    float targetNearest[3]{};
    bool targetOver = false;
    if (dtStatusFailed(query->findNearestPoly(targetPos, searchExtents, &filter, &targetRef, targetNearest, &targetOver)) || targetRef == 0)
    {
        printf("[NavMeshData] AddOffmeshLinksToNavMeshIsland: nao encontrou poly pro target.\n");
        return false;
    }
    printf("[IslandOffmesh] Target poly found\n");

    int targetTileX = -1;
    int targetTileY = -1;
    const dtMeshTile* targetTile = nullptr;
    const dtPoly* targetPoly = nullptr;
    if (dtStatusSucceed(m_nav->getTileAndPolyByRef(targetRef, &targetTile, &targetPoly)))
    {
        if (targetTile && targetTile->header)
        {
            targetTileX = targetTile->header->x;
            targetTileY = targetTile->header->y;
        }
    }

    std::vector<dtPolyRef> stack;
    stack.push_back(targetRef);
    std::unordered_set<dtPolyRef> islandPolys;
    islandPolys.reserve(256);

    while (!stack.empty())
    {
        dtPolyRef ref = stack.back();
        stack.pop_back();
        if (islandPolys.find(ref) != islandPolys.end())
            continue;
        islandPolys.insert(ref);

        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        if (dtStatusFailed(m_nav->getTileAndPolyByRef(ref, &tile, &poly)))
            continue;
        if (!tile || !poly)
            continue;

        for (int edge = 0; edge < poly->vertCount; ++edge)
        {
            const dtPolyRef nei = GetNeighbourRef(tile, poly, edge, m_nav);
            if (nei != 0 && islandPolys.find(nei) == islandPolys.end())
                stack.push_back(nei);
        }
    }
    printf("[IslandOffmesh] Island polys = %zu\n", islandPolys.size());

    OrientedRectXZ rect{};
    rect.center = params.targetPosition;
    rect.extents = glm::abs(params.searchExtents);
    const float angleRad = glm::radians(params.searchAngleDegrees);
    rect.sinAngle = sinf(angleRad);
    rect.cosAngle = cosf(angleRad);

    float queryBMin[3]{};
    float queryBMax[3]{};
    rect.ComputeAabb(queryBMin, queryBMax);

    const float queryCenter[3] = {
        (queryBMin[0] + queryBMax[0]) * 0.5f,
        (queryBMin[1] + queryBMax[1]) * 0.5f,
        (queryBMin[2] + queryBMax[2]) * 0.5f
    };
    const float queryHalfExtents[3] = {
        (queryBMax[0] - queryBMin[0]) * 0.5f,
        (queryBMax[1] - queryBMin[1]) * 0.5f,
        (queryBMax[2] - queryBMin[2]) * 0.5f
    };

    const int maxPolys = 4096;
    dtPolyRef polys[maxPolys];
    int polyCount = 0;
    if (dtStatusFailed(query->queryPolygons(queryCenter, queryHalfExtents, &filter, polys, &polyCount, maxPolys)))
        return false;

    struct Candidate
    {
        glm::vec3 mid{0.0f};
        glm::vec3 islandPoint{0.0f};
        float distance = 0.0f;
        float heightDiff = 0.0f;
        dtPolyRef ref = 0;
        int tileX = -1;
        int tileY = -1;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(static_cast<size_t>(polyCount));

    auto resolveTileCoords = [&](const dtMeshTile* tile, const glm::vec3& pos) -> std::pair<int, int>
    {
        if (tile && tile->header)
            return {tile->header->x, tile->header->y};

        const float tileWidth = m_cachedSettings.tileSize * m_cachedBaseCfg.cs;
        if (m_hasTiledCache && tileWidth > 0.0f)
        {
            const int tx = (int)floorf((pos.x - m_gridBMin[0]) / tileWidth);
            const int ty = (int)floorf((pos.z - m_gridBMin[2]) / tileWidth);
            return {tx, ty};
        }

        return {-1, -1};
    };

    if (targetTileX == -1 || targetTileY == -1)
    {
        const auto coords = resolveTileCoords(targetTile, params.targetPosition);
        targetTileX = coords.first;
        targetTileY = coords.second;
    }

    for (int i = 0; i < polyCount; ++i)
    {
        const dtPolyRef pref = polys[i];
        if (islandPolys.find(pref) != islandPolys.end())
            continue;

        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        if (dtStatusFailed(m_nav->getTileAndPolyByRef(pref, &tile, &poly)))
            continue;
        if (!tile || !poly || poly->getType() != DT_POLYTYPE_GROUND)
            continue;

        const int vertCount = poly->vertCount;
        for (int edge = 0; edge < vertCount; ++edge)
        {
            const dtPolyRef nei = GetNeighbourRef(tile, poly, edge, m_nav);
            if (nei != 0)
                continue;

            const int vaIndex = edge;
            const int vbIndex = (edge + 1) % vertCount;
            const glm::vec3 va = GetPolyVertex(tile, poly, vaIndex);
            const glm::vec3 vb = GetPolyVertex(tile, poly, vbIndex);
            const glm::vec3 mid = (va + vb) * 0.5f;
            if (!rect.Contains(mid))
                continue;

            float closest[3]{};
            bool overPoly = false;
            if (dtStatusFailed(query->closestPointOnPoly(targetRef, &mid.x, closest, &overPoly)))
                continue;

            const glm::vec3 islandPoint(closest[0], closest[1], closest[2]);
            const float dist = glm::length(mid - islandPoint);
            if (dist < params.minDistance || dist > params.maxDistance)
                continue;

            const float heightDiff = mid.y - islandPoint.y;
            if (fabsf(heightDiff) > params.maxHeightDiff)
                continue;

            const auto tileCoords = resolveTileCoords(tile, mid);
            candidates.push_back({mid, islandPoint, dist, heightDiff, pref, tileCoords.first, tileCoords.second});
        }
    }
    printf("[IslandOffmesh] Candidates found = %zu\n", candidates.size());

    if (candidates.empty())
        return true;

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
    {
        return a.distance < b.distance;
    });

    const int desiredLinks = 1 + std::max(0, params.moreLinks);
    std::vector<Candidate> selected;
    for (const auto& cand : candidates)
    {
        bool tooClose = false;
        for (const auto& chosen : selected)
        {
            if (glm::length(chosen.mid - cand.mid) < params.minDistance)
            {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;
        selected.push_back(cand);
        if ((int)selected.size() >= desiredLinks)
            break;
    }

    if (selected.empty())
        return true;

    size_t generated = 0;
    for (const auto& cand : selected)
    {
        glm::vec3 hit;
        glm::vec3 normal;
        if (RaycastTo(m_cachedVerts, m_cachedTris, cand.islandPoint, cand.mid, hit, normal))
            continue;

        if (params.linkDown)
        {
            OffmeshLink link{};
            link.start = cand.islandPoint;
            link.end = cand.mid;
            link.radius = 1.0f;
            link.bidirectional = false;
            link.area = params.areaDrop;
            link.flags = 1;
            link.ownerTx = targetTileX;
            link.ownerTy = targetTileY;
            outLinks.push_back(link);
            ++generated;
        }

        if (params.linkUp)
        {
            OffmeshLink link{};
            link.start = cand.mid;
            link.end = cand.islandPoint;
            link.radius = 1.0f;
            link.bidirectional = false;
            link.area = params.areaJump;
            link.flags = 1;
            link.ownerTx = cand.tileX;
            link.ownerTy = cand.tileY;
            outLinks.push_back(link);
            ++generated;
        }
    }

    printf("[IslandOffmesh] Links generated = %zu\n", generated);
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
        std::memcpy(m_gridBMin, other.m_gridBMin, sizeof(m_gridBMin));
        std::memcpy(m_gridBMax, other.m_gridBMax, sizeof(m_gridBMax));
        m_cachedBaseCfg = other.m_cachedBaseCfg;
        m_cachedSettings = other.m_cachedSettings;
        m_cachedTileWidthCount = other.m_cachedTileWidthCount;
        m_cachedTileHeightCount = other.m_cachedTileHeightCount;
        m_hasTiledCache = other.m_hasTiledCache;
        m_cachedTileHashes = std::move(other.m_cachedTileHashes);
        m_offmeshLinks = std::move(other.m_offmeshLinks);
        m_fixedGridBounds = other.m_fixedGridBounds;

        std::memset(other.m_cachedBMin, 0, sizeof(other.m_cachedBMin));
        std::memset(other.m_cachedBMax, 0, sizeof(other.m_cachedBMax));
        std::memset(other.m_gridBMin, 0, sizeof(other.m_gridBMin));
        std::memset(other.m_gridBMax, 0, sizeof(other.m_gridBMax));
        other.m_cachedTileWidthCount = 0;
        other.m_cachedTileHeightCount = 0;
        other.m_hasTiledCache = false;
        other.m_cachedTileHashes.clear();
        other.m_fixedGridBounds = false;
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
    const int tx = (int)floorf((worldPos.x - m_gridBMin[0]) / tileWidth);
    const int ty = (int)floorf((worldPos.z - m_gridBMin[2]) / tileWidth);

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
    rcVcopy(buildInput.meshBMin, m_gridBMin);
    rcVcopy(buildInput.meshBMax, m_gridBMax);
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
    const int tx = (int)floorf((worldPos.x - m_gridBMin[0]) / tileWidth);
    const int ty = (int)floorf((worldPos.z - m_gridBMin[2]) / tileWidth);

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
    int minTx = (int)floorf((bmin.x - m_gridBMin[0]) / tileWidth);
    int minTy = (int)floorf((bmin.z - m_gridBMin[2]) / tileWidth);
    int maxTx = (int)floorf((bmax.x - m_gridBMin[0]) / tileWidth);
    int maxTy = (int)floorf((bmax.z - m_gridBMin[2]) / tileWidth);

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
    rcVcopy(buildInput.meshBMin, m_gridBMin);
    rcVcopy(buildInput.meshBMax, m_gridBMax);
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

bool NavMeshData::EstimateTileGrid(const NavmeshGenerationSettings& settings,
                                   const float* bmin,
                                   const float* bmax,
                                   TileGridStats& outStats)
{
    return ComputeTiledGridCounts(settings, bmin, bmax, outStats);
}

bool NavMeshData::InitTiledGrid(const NavmeshGenerationSettings& settings,
                                const float* forcedBMin,
                                const float* forcedBMax)
{
    if (settings.mode != NavmeshBuildMode::Tiled)
    {
        printf("[NavMeshData] InitTiledGrid: modo nao e tiled.\n");
        return false;
    }

    if (!forcedBMin || !forcedBMax)
    {
        printf("[NavMeshData] InitTiledGrid: bounds obrigatorios para grid fixo.\n");
        return false;
    }

    if (m_nav)
    {
        dtFreeNavMesh(m_nav);
        m_nav = nullptr;
    }

    TileGridStats stats{};
    if (!ComputeTiledGridCounts(settings, forcedBMin, forcedBMax, stats))
    {
        printf("[NavMeshData] InitTiledGrid: parametros invalidos para grid.\n");
        return false;
    }

    const int maxAllowedTiles = 32768;
    if (stats.tileCountTotal > maxAllowedTiles)
    {
        const float area = std::max(0.0f, stats.boundsWidth * stats.boundsHeight);
        const float minTileWorld = area > 0.0f
            ? std::sqrt(area / static_cast<float>(maxAllowedTiles))
            : stats.tileWorld;
        const int suggestedTileSize = static_cast<int>(std::ceil(minTileWorld / std::max(0.001f, settings.cellSize)));

        printf("[NavMeshData] InitTiledGrid: quantidade de tiles (%d) excede limite seguro (%d). tileWorld=%.2f. Sugestao: tileWorld >= %.2f (tileSize >= %d com cellSize=%.3f).\n",
               stats.tileCountTotal, maxAllowedTiles, stats.tileWorld, minTileWorld, suggestedTileSize, settings.cellSize);
        return false;
    }

    rcConfig baseCfg{};
    FillBaseConfig(settings, baseCfg);
    rcCalcGridSize(forcedBMin, forcedBMax, baseCfg.cs, &baseCfg.width, &baseCfg.height);
    if (baseCfg.width == 0 || baseCfg.height == 0)
    {
        printf("[NavMeshData] InitTiledGrid: rcCalcGridSize retornou grade invalida (%d x %d).\n",
               baseCfg.width, baseCfg.height);
        return false;
    }

    const int tileWidthCount = (baseCfg.width + settings.tileSize - 1) / settings.tileSize;
    const int tileHeightCount = (baseCfg.height + settings.tileSize - 1) / settings.tileSize;

    dtNavMesh* nav = dtAllocNavMesh();
    if (!nav)
    {
        printf("[NavMeshData] InitTiledGrid: dtAllocNavMesh falhou.\n");
        return false;
    }

    dtNavMeshParams navParams{};
    rcVcopy(navParams.orig, forcedBMin);
    navParams.tileWidth = settings.tileSize * baseCfg.cs;
    navParams.tileHeight = settings.tileSize * baseCfg.cs;
    navParams.maxTiles = tileWidthCount * tileHeightCount;

    const unsigned int desiredMaxPolys = 2048;
    const unsigned int tileBits = static_cast<unsigned int>(dtIlog2(dtNextPow2(static_cast<unsigned int>(navParams.maxTiles))));
    const unsigned int desiredPolyBits = static_cast<unsigned int>(dtIlog2(dtNextPow2(desiredMaxPolys)));
    const unsigned int maxPolyBitsAllowed = tileBits >= 22 ? 0u : (22u - tileBits);
    if (maxPolyBitsAllowed == 0)
    {
        printf("[NavMeshData] InitTiledGrid: maxTiles=%u consome todos os bits de ref (tileBits=%u). Ajuste tileSize ou reduza a area.\n",
               static_cast<unsigned int>(navParams.maxTiles), tileBits);
        dtFreeNavMesh(nav);
        return false;
    }

    const unsigned int chosenPolyBits = std::min(desiredPolyBits, maxPolyBitsAllowed);
    navParams.maxPolys = 1u << chosenPolyBits;
    if (navParams.maxPolys != desiredMaxPolys)
    {
        printf("[NavMeshData] InitTiledGrid: Clamp maxPolys para %u (tileBits=%u polyBits=%u). Numero de tiles=%u\n",
               static_cast<unsigned int>(navParams.maxPolys), tileBits, chosenPolyBits, static_cast<unsigned int>(navParams.maxTiles));
    }

    const dtStatus initStatus = nav->init(&navParams);
    if (dtStatusFailed(initStatus))
    {
        printf("[NavMeshData] InitTiledGrid: m_nav->init falhou (tiled). status=0x%x maxTiles=%d maxPolys=%d tileWidth=%.3f tileHeight=%.3f orig=(%.2f, %.2f, %.2f)\n",
               initStatus, navParams.maxTiles, navParams.maxPolys, navParams.tileWidth, navParams.tileHeight,
               navParams.orig[0], navParams.orig[1], navParams.orig[2]);
        dtFreeNavMesh(nav);
        return false;
    }

    m_nav = nav;
    m_cachedVerts.clear();
    m_cachedTris.clear();
    rcVcopy(m_gridBMin, forcedBMin);
    rcVcopy(m_gridBMax, forcedBMax);
    rcVcopy(m_cachedBMin, forcedBMin);
    rcVcopy(m_cachedBMax, forcedBMax);
    m_cachedBaseCfg = baseCfg;
    m_cachedSettings = settings;
    m_cachedTileWidthCount = tileWidthCount;
    m_cachedTileHeightCount = tileHeightCount;
    m_cachedTileHashes.clear();
    m_hasTiledCache = true;
    m_fixedGridBounds = true;
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

    float meshBMin[3] = { convertedVerts[0], convertedVerts[1], convertedVerts[2] };
    float meshBMax[3] = { convertedVerts[0], convertedVerts[1], convertedVerts[2] };
    for (size_t i = 1; i < nverts; ++i)
    {
        const float* v = &convertedVerts[i * 3];
        if (v[0] < meshBMin[0]) meshBMin[0] = v[0];
        if (v[1] < meshBMin[1]) meshBMin[1] = v[1];
        if (v[2] < meshBMin[2]) meshBMin[2] = v[2];
        if (v[0] > meshBMax[0]) meshBMax[0] = v[0];
        if (v[1] > meshBMax[1]) meshBMax[1] = v[1];
        if (v[2] > meshBMax[2]) meshBMax[2] = v[2];
    }
    rcVcopy(m_cachedBMin, meshBMin);
    rcVcopy(m_cachedBMax, meshBMax);
    if (!m_fixedGridBounds)
    {
        rcVcopy(m_gridBMin, meshBMin);
        rcVcopy(m_gridBMax, meshBMax);
    }

    m_cachedVerts = std::move(convertedVerts);
    m_cachedTris = std::move(convertedTris);
    m_cachedTileHashes.clear();
    return true;
}

bool NavMeshData::GetCachedBounds(float* outBMin, float* outBMax) const
{
    if (!m_nav)
        return false;
    if (outBMin)
        rcVcopy(outBMin, m_gridBMin);
    if (outBMax)
        rcVcopy(outBMax, m_gridBMax);
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
                               const char* cachePath,
                               const float* forcedBMin,
                               const float* forcedBMax)
{
    m_hasTiledCache = false;
    m_cachedTileHashes.clear();
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

    float geomBMin[3] = { verts[0], verts[1], verts[2] };
    float geomBMax[3] = { verts[0], verts[1], verts[2] };
    for (int i = 1; i < nverts; ++i)
    {
        const float* v = &verts[i*3];
        if (v[0] < geomBMin[0]) geomBMin[0] = v[0];
        if (v[1] < geomBMin[1]) geomBMin[1] = v[1];
        if (v[2] < geomBMin[2]) geomBMin[2] = v[2];
        if (v[0] > geomBMax[0]) geomBMax[0] = v[0];
        if (v[1] > geomBMax[1]) geomBMax[1] = v[1];
        if (v[2] > geomBMax[2]) geomBMax[2] = v[2];
    }
    float gridBMin[3] = { geomBMin[0], geomBMin[1], geomBMin[2] };
    float gridBMax[3] = { geomBMax[0], geomBMax[1], geomBMax[2] };
    const bool useForcedGrid = (settings.mode == NavmeshBuildMode::Tiled && forcedBMin && forcedBMax);
    if (useForcedGrid)
    {
        rcVcopy(gridBMin, forcedBMin);
        rcVcopy(gridBMax, forcedBMax);
    }

    rcConfig baseCfg{};
    FillBaseConfig(settings, baseCfg);

    const float* gridMin = (settings.mode == NavmeshBuildMode::Tiled) ? gridBMin : geomBMin;
    const float* gridMax = (settings.mode == NavmeshBuildMode::Tiled) ? gridBMax : geomBMax;
    rcCalcGridSize(gridMin, gridMax, baseCfg.cs, &baseCfg.width, &baseCfg.height);
    if (baseCfg.width == 0 || baseCfg.height == 0)
    {
        printf("[NavMeshData] BuildFromMesh: rcCalcGridSize retornou grade invalida (%d x %d).\n",
               baseCfg.width, baseCfg.height);
        return false;
    }

    NavmeshBuildInput buildInput{ctx, verts, tris, nverts, ntris};
    rcVcopy(buildInput.meshBMin, gridMin);
    rcVcopy(buildInput.meshBMax, gridMax);
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
        ok = BuildTiledNavMesh(buildInput, settings, newNav, buildTilesNow, nullptr, nullptr, cancelFlag, useCache, cachePath, &m_cachedTileHashes);
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
    rcVcopy(m_cachedBMin, geomBMin);
    rcVcopy(m_cachedBMax, geomBMax);
    rcVcopy(m_gridBMin, gridMin);
    rcVcopy(m_gridBMax, gridMax);
    m_cachedBaseCfg = baseCfg;
    m_cachedSettings = settings;
    m_fixedGridBounds = useForcedGrid;
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
        m_cachedTileHashes.clear();
    }
    return true;
}
