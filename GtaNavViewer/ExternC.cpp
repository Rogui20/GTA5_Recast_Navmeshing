#include "ExternC.h"
#include "NavMesh_TileCacheDB.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace
{
    struct LoadedGeometry
    {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
        glm::vec3 bmin{FLT_MAX};
        glm::vec3 bmax{-FLT_MAX};
        bool Valid() const { return !vertices.empty() && !indices.empty(); }
    };

    struct GeometryInstance
    {
        std::string id;
        LoadedGeometry source;
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f}; // graus
        glm::vec3 worldBMin{0.0f};
        glm::vec3 worldBMax{0.0f};
    };

    struct ExternNavmeshContext
    {
        NavmeshGenerationSettings genSettings{};
        std::vector<GeometryInstance> geometries;
        std::vector<OffmeshLink> offmeshLinks;
        AutoOffmeshGenerationParams autoOffmeshParams{};
        NavMeshData navData;
        dtNavMeshQuery* navQuery = nullptr;
        bool hasBoundingBox = false;
        glm::vec3 bboxMin{0.0f};
        glm::vec3 bboxMax{0.0f};
        bool rebuildAll = false;
        std::vector<std::pair<glm::vec3, glm::vec3>> dirtyBounds;
        float cachedExtents[3]{20.0f, 10.0f, 20.0f};
        std::string cacheRoot;
        std::string sessionId;
        int maxResidentTiles = 256;
        bool streamingEnabled = false;
        std::unordered_map<uint64_t, uint32_t> residentStamp;
        std::unordered_set<uint64_t> residentTiles;
        uint32_t stampCounter = 0;
        std::unordered_map<uint64_t, TileDbIndexEntry> dbIndexCache;
        bool dbIndexLoaded = false;
        std::filesystem::file_time_type dbMTime{};
    };

    glm::mat3 GetRotationMatrix(const glm::vec3& eulerDegrees)
    {
        glm::vec3 normalized = glm::vec3(
            std::fmod(eulerDegrees.x, 360.0f),
            std::fmod(eulerDegrees.y, 360.0f),
            std::fmod(eulerDegrees.z, 360.0f));
        glm::mat4 rot(1.0f);
        rot = glm::rotate(rot, glm::radians(normalized.z), glm::vec3(0, 1, 0));
        rot = glm::rotate(rot, glm::radians(normalized.x), glm::vec3(1, 0, 0));
        rot = glm::rotate(rot, glm::radians(normalized.y), glm::vec3(0, 0, 1));
        return glm::mat3(rot);
    }

    void UpdateWorldBounds(GeometryInstance& instance)
    {
        glm::vec3 bmin(FLT_MAX);
        glm::vec3 bmax(-FLT_MAX);
        glm::mat3 rot = GetRotationMatrix(instance.rotation);

        for (const auto& v : instance.source.vertices)
        {
            glm::vec3 world = rot * v + instance.position;
            bmin = glm::min(bmin, world);
            bmax = glm::max(bmax, world);
        }

        instance.worldBMin = bmin;
        instance.worldBMax = bmax;
    }

    LoadedGeometry LoadBin(const std::filesystem::path& path)
    {
        LoadedGeometry geom;
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return geom;

        uint32_t version = 0;
        uint64_t vertexCount = 0;
        uint64_t indexCount = 0;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        in.read(reinterpret_cast<char*>(&geom.bmin), sizeof(geom.bmin));
        in.read(reinterpret_cast<char*>(&geom.bmax), sizeof(geom.bmax));
        if (!in.good() || version != 1)
            return LoadedGeometry{};

        geom.vertices.resize(vertexCount);
        geom.indices.resize(indexCount);
        in.read(reinterpret_cast<char*>(geom.vertices.data()), sizeof(glm::vec3) * vertexCount);
        in.read(reinterpret_cast<char*>(geom.indices.data()), sizeof(unsigned int) * indexCount);
        if (!in.good())
            return LoadedGeometry{};

        return geom;
    }

    struct TempVertex
    {
        int v = 0;
        int vt = 0;
        int vn = 0;
    };

    TempVertex ParseFaceElement(const std::string& token)
    {
        TempVertex tv;
        sscanf(token.c_str(), "%d/%d/%d", &tv.v, &tv.vt, &tv.vn);
        if (token.find("//") != std::string::npos)
        {
            sscanf(token.c_str(), "%d//%d", &tv.v, &tv.vn);
            tv.vt = 0;
        }
        return tv;
    }

    bool SaveGeometryToBin(const std::filesystem::path& path, const LoadedGeometry& geom)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
        {
            std::cout << "BIN: failed to open for write " << path << "\n";
            return false;
        }

        uint32_t version = 1;
        uint64_t vertexCount = geom.vertices.size();
        uint64_t indexCount = geom.indices.size();

        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        out.write(reinterpret_cast<const char*>(&geom.bmin), sizeof(geom.bmin));
        out.write(reinterpret_cast<const char*>(&geom.bmax), sizeof(geom.bmax));

        out.write(reinterpret_cast<const char*>(geom.vertices.data()), sizeof(glm::vec3) * vertexCount);
        out.write(reinterpret_cast<const char*>(geom.indices.data()), sizeof(unsigned int) * indexCount);

        return true;
    }

    LoadedGeometry LoadObj(const std::filesystem::path& path)
    {
        LoadedGeometry geom;
        std::ifstream file(path);
        if (!file.is_open())
            return geom;

        std::string line;
        std::vector<glm::vec3> originalVerts;
        std::vector<unsigned int> indices;

        glm::vec3 navMinB(FLT_MAX);
        glm::vec3 navMaxB(-FLT_MAX);

        while (std::getline(file, line))
        {
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v")
            {
                float x, y, z;
                ss >> x >> y >> z;
                glm::vec3 v(x, y, z);
                originalVerts.push_back(v);
                navMinB = glm::min(navMinB, v);
                navMaxB = glm::max(navMaxB, v);
            }
            else if (type == "f")
            {
                std::vector<TempVertex> faceVerts;
                std::string token;
                while (ss >> token)
                {
                    TempVertex tv = ParseFaceElement(token);
                    if (tv.v < 0)
                    {
                        tv.v = static_cast<int>(originalVerts.size()) + tv.v + 1;
                    }
                    faceVerts.push_back(tv);
                }

                if (faceVerts.size() >= 3)
                {
                    for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
                    {
                        const TempVertex& v0 = faceVerts[0];
                        const TempVertex& v1 = faceVerts[i];
                        const TempVertex& v2 = faceVerts[i + 1];
                        if (v0.v <= 0 || v1.v <= 0 || v2.v <= 0)
                            continue;
                        indices.push_back(static_cast<unsigned int>(v0.v - 1));
                        indices.push_back(static_cast<unsigned int>(v1.v - 1));
                        indices.push_back(static_cast<unsigned int>(v2.v - 1));
                    }
                }
            }
        }

        geom.vertices = std::move(originalVerts);
        geom.indices = std::move(indices);
        geom.bmin = navMinB;
        geom.bmax = navMaxB;
        if (!geom.vertices.empty() && geom.indices.empty())
        {
            std::cout << "OBJ: " << path << " carregado com vertices, mas sem indices. Verifique formato dos faces.\n";
        }
        return geom;
    }

    LoadedGeometry LoadGeometry(const char* path, bool preferBin)
    {
        if (!path)
            return {};

        std::filesystem::path p(path);
        std::filesystem::path binPath = p;
        std::filesystem::path objPath = p;
        if (p.extension() == ".bin")
        {
            objPath.replace_extension(".obj");
        }
        else
        {
            binPath.replace_extension(".bin");
        }

        if (preferBin)
        {
            if (std::filesystem::exists(binPath))
            {
                auto geom = LoadBin(binPath);
                if (geom.Valid())
                    return geom;
            }

            auto geom = LoadObj(objPath);
            if (geom.Valid())
            {
                if (!std::filesystem::exists(binPath) && SaveGeometryToBin(binPath, geom))
                {
                    std::cout << "BIN salvo em " << binPath << "\n";
                }
                return geom;
            }
            return {};
        }

        if (std::filesystem::exists(p))
        {
            if (p.extension() == ".bin")
            {
                return LoadBin(p);
            }
            return LoadObj(p);
        }

        return {};
    }

    bool CombineGeometry(const ExternNavmeshContext& ctx,
                         std::vector<glm::vec3>& outVerts,
                         std::vector<unsigned int>& outIndices)
    {
        outVerts.clear();
        outIndices.clear();

        if (ctx.geometries.empty())
            return false;

        for (const auto& inst : ctx.geometries)
        {
            if (!inst.source.Valid())
                continue;

            const unsigned int baseIndex = static_cast<unsigned int>(outVerts.size());
            glm::mat3 rot = GetRotationMatrix(inst.rotation);
            std::vector<glm::vec3> transformed;
            transformed.reserve(inst.source.vertices.size());
            for (const auto& v : inst.source.vertices)
                transformed.push_back(rot * v + inst.position);

            if (!ctx.hasBoundingBox)
            {
                outVerts.insert(outVerts.end(), transformed.begin(), transformed.end());
                for (size_t i = 0; i < inst.source.indices.size(); i += 3)
                {
                    unsigned int i0 = inst.source.indices[i + 0];
                    unsigned int i1 = inst.source.indices[i + 1];
                    unsigned int i2 = inst.source.indices[i + 2];
                    outIndices.push_back(baseIndex + i0);
                    outIndices.push_back(baseIndex + i1);
                    outIndices.push_back(baseIndex + i2);
                }
                continue;
            }

            auto triOverlapsBBox = [&](const glm::vec3& triMin, const glm::vec3& triMax)
            {
                if (triMin.x > ctx.bboxMax.x || triMax.x < ctx.bboxMin.x) return false;
                if (triMin.y > ctx.bboxMax.y || triMax.y < ctx.bboxMin.y) return false;
                if (triMin.z > ctx.bboxMax.z || triMax.z < ctx.bboxMin.z) return false;
                return true;
            };

            std::unordered_map<unsigned int, unsigned int> remap;
            remap.reserve(inst.source.vertices.size());
            auto mapVertex = [&](unsigned int localIdx) -> unsigned int
            {
                auto it = remap.find(localIdx);
                if (it != remap.end())
                    return it->second;
                unsigned int newIdx = static_cast<unsigned int>(outVerts.size());
                remap.emplace(localIdx, newIdx);
                outVerts.push_back(transformed[localIdx]);
                return newIdx;
            };

            for (size_t i = 0; i < inst.source.indices.size(); i += 3)
            {
                unsigned int i0 = inst.source.indices[i + 0];
                unsigned int i1 = inst.source.indices[i + 1];
                unsigned int i2 = inst.source.indices[i + 2];

                const glm::vec3& v0 = transformed[i0];
                const glm::vec3& v1 = transformed[i1];
                const glm::vec3& v2 = transformed[i2];
                glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
                glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);
                if (!triOverlapsBBox(triMin, triMax))
                    continue;

                unsigned int ni0 = mapVertex(i0);
                unsigned int ni1 = mapVertex(i1);
                unsigned int ni2 = mapVertex(i2);
                outIndices.push_back(ni0);
                outIndices.push_back(ni1);
                outIndices.push_back(ni2);
            }
        }

        return !outVerts.empty() && !outIndices.empty();
    }

    std::filesystem::path GetSessionCachePath(const ExternNavmeshContext& ctx)
    {
        const char* defaultSessionId = "Navmesh_01";
        std::string session = ctx.sessionId.empty() ? defaultSessionId : ctx.sessionId;
        std::filesystem::path root = ctx.cacheRoot.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(ctx.cacheRoot);
        return root / (session + ".tilecache");
    }

    bool EnsureDbIndexLoaded(ExternNavmeshContext& ctx, const std::filesystem::path& cachePath)
    {
        if (!ctx.navData.GetNavMesh())
            return false;

        if (!std::filesystem::exists(cachePath))
        {
            ctx.dbIndexCache.clear();
            ctx.dbIndexLoaded = false;
            ctx.dbMTime = {};
            return false;
        }

        const auto mtime = std::filesystem::last_write_time(cachePath);
        if (!ctx.dbIndexLoaded || ctx.dbMTime != mtime)
        {
            ctx.dbIndexCache.clear();
            if (!TileDbLoadIndex(cachePath.string().c_str(), ctx.navData.GetNavMesh(), ctx.dbIndexCache))
            {
                ctx.dbIndexLoaded = false;
                ctx.dbMTime = mtime;
                return false;
            }
            ctx.dbIndexLoaded = true;
            ctx.dbMTime = mtime;
        }

        return ctx.dbIndexLoaded;
    }

    bool EnsureNavQuery(ExternNavmeshContext& ctx)
    {
        if (!ctx.navData.GetNavMesh())
            return false;

        if (!ctx.navQuery)
        {
            ctx.navQuery = dtAllocNavMeshQuery();
            if (!ctx.navQuery)
                return false;
        }

        if (dtStatusFailed(ctx.navQuery->init(ctx.navData.GetNavMesh(), 2048)))
            return false;

        const float extents[3] = {
            20.0f,
            10.5f,
            20.0f
        };
        memcpy(ctx.cachedExtents, extents, sizeof(extents));
        return true;
    }

    GeometryInstance* FindGeometry(ExternNavmeshContext& ctx, const char* id)
    {
        if (!id) return nullptr;
        for (auto& g : ctx.geometries)
        {
            if (g.id == id)
                return &g;
        }
        return nullptr;
    }

    void RegisterDirtyBounds(ExternNavmeshContext& ctx, const GeometryInstance& inst)
    {
        ctx.dirtyBounds.emplace_back(inst.worldBMin, inst.worldBMax);
    }

    bool RebuildDirtyTiles(ExternNavmeshContext& ctx)
    {
        if (ctx.rebuildAll || !ctx.navData.IsLoaded() || !ctx.navData.HasTiledCache())
            return false;

        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        if (!CombineGeometry(ctx, verts, indices))
            return false;

        if (!ctx.navData.UpdateCachedGeometry(verts, indices))
            return false;

        std::set<uint64_t> dirtyTiles;
        const auto tileKey = [](int tx, int ty) -> uint64_t
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
        };

        for (const auto& bounds : ctx.dirtyBounds)
        {
            std::vector<std::pair<int, int>> tiles;
            if (!ctx.navData.CollectTilesInBounds(bounds.first, bounds.second, true, tiles))
                continue;
            for (const auto& t : tiles)
                dirtyTiles.insert(tileKey(t.first, t.second));
        }

        if (dirtyTiles.empty())
        {
            ctx.dirtyBounds.clear();
            return true;
        }

        std::vector<std::pair<int, int>> toRebuild;
        toRebuild.reserve(dirtyTiles.size());
        for (uint64_t key : dirtyTiles)
        {
            int tx = static_cast<int>(key >> 32);
            int ty = static_cast<int>(key & 0xffffffffu);
            toRebuild.emplace_back(tx, ty);
        }

        bool ok = ctx.navData.RebuildSpecificTiles(toRebuild, ctx.genSettings, true, nullptr);
        if (ok)
            ctx.dirtyBounds.clear();
        return ok;
    }

    bool BuildNavmeshInternal(ExternNavmeshContext& ctx)
    {
        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        if (!CombineGeometry(ctx, verts, indices))
            return false;

        ctx.rebuildAll = false;
        ctx.dirtyBounds.clear();
        ctx.navData.SetOffmeshLinks(ctx.offmeshLinks);

        const bool isTiled = ctx.genSettings.mode == NavmeshBuildMode::Tiled;
        std::filesystem::path cachePath = GetSessionCachePath(ctx);
        const float* forcedBMin = nullptr;
        const float* forcedBMax = nullptr;
        float forcedMin[3];
        float forcedMax[3];
        if (isTiled && ctx.hasBoundingBox)
        {
            forcedMin[0] = ctx.bboxMin.x;
            forcedMin[1] = ctx.bboxMin.y;
            forcedMin[2] = ctx.bboxMin.z;
            forcedMax[0] = ctx.bboxMax.x;
            forcedMax[1] = ctx.bboxMax.y;
            forcedMax[2] = ctx.bboxMax.z;
            forcedBMin = forcedMin;
            forcedBMax = forcedMax;
        }

        if (!ctx.navData.BuildFromMesh(verts, indices, ctx.genSettings, isTiled, nullptr, true, cachePath.string().c_str(), forcedBMin, forcedBMax))
            return false;

        return EnsureNavQuery(ctx);
    }

    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild)
    {
        if (ctx.streamingEnabled)
        {
            if (!ctx.navData.IsLoaded() || !ctx.navData.HasTiledCache())
            {
                if (!ctx.hasBoundingBox)
                {
                    printf("[NavMeshData] UpdateNavmeshState: streaming mode sem bounds fixos.\n");
                    return false;
                }

                float forcedMin[3] = { ctx.bboxMin.x, ctx.bboxMin.y, ctx.bboxMin.z };
                float forcedMax[3] = { ctx.bboxMax.x, ctx.bboxMax.y, ctx.bboxMax.z };
                ctx.navData.SetOffmeshLinks(ctx.offmeshLinks);
                if (!ctx.navData.InitTiledGrid(ctx.genSettings, forcedMin, forcedMax))
                    return false;
            }

            if (!ctx.dirtyBounds.empty())
            {
                if (!RebuildDirtyTiles(ctx))
                    return false;
            }

            ctx.rebuildAll = false;
            return EnsureNavQuery(ctx);
        }

        if (forceFullBuild || ctx.rebuildAll || !ctx.navData.IsLoaded())
            return BuildNavmeshInternal(ctx);

        if (ctx.genSettings.mode != NavmeshBuildMode::Tiled || !ctx.navData.HasTiledCache())
            return BuildNavmeshInternal(ctx);

        if (!RebuildDirtyTiles(ctx))
            return BuildNavmeshInternal(ctx);

        return EnsureNavQuery(ctx);
    }


    Vector3 ToVector3(const glm::vec3& v)
    {
        return Vector3{v.x, v.y, v.z};
    }

    glm::vec3 ComputePolyNormal(const dtMeshTile* tile, const dtPoly* poly)
    {
        if (!tile || !poly || poly->vertCount < 3)
            return glm::vec3(0.0f, 1.0f, 0.0f);

        const int i0 = poly->verts[0];
        const int i1 = poly->verts[1];
        const int i2 = poly->verts[2];
        const float* v0 = &tile->verts[i0 * 3];
        const float* v1 = &tile->verts[i1 * 3];
        const float* v2 = &tile->verts[i2 * 3];
        const glm::vec3 a(v0[0], v0[1], v0[2]);
        const glm::vec3 b(v1[0], v1[1], v1[2]);
        const glm::vec3 c(v2[0], v2[1], v2[2]);
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z))
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        return normal;
    }

    glm::vec3 ComputePolyCentroid(const dtMeshTile* tile, const dtPoly* poly)
    {
        glm::vec3 sum(0.0f);
        for (int i = 0; i < poly->vertCount; ++i)
        {
            const int vi = poly->verts[i];
            const float* v = &tile->verts[vi * 3];
            sum += glm::vec3(v[0], v[1], v[2]);
        }
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
        const glm::vec3 edgeCenter = (a + b) * 0.5f;
        const glm::vec3 toCenter = glm::normalize(edgeCenter - polyCenter);
        if (glm::dot(outward, toCenter) > 0.0f)
            outward = -outward;
        if (!std::isfinite(outward.x) || !std::isfinite(outward.y) || !std::isfinite(outward.z))
            outward = glm::vec3(0.0f, 0.0f, 0.0f);
        return outward;
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
}

GTANAVVIEWER_API void* InitNavMesh()
{
    auto* ctx = new ExternNavmeshContext();
    return ctx;
}

GTANAVVIEWER_API void DestroyNavMeshResources(void* navMesh)
{
    if (!navMesh)
        return;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->navQuery)
    {
        dtFreeNavMeshQuery(ctx->navQuery);
        ctx->navQuery = nullptr;
    }

    delete ctx;
}

GTANAVVIEWER_API void SetNavMeshGenSettings(void* navMesh, const NavmeshGenerationSettings* settings)
{
    if (!navMesh || !settings)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->genSettings = *settings;
    ctx->autoOffmeshParams.agentRadius = settings->agentRadius;
    ctx->autoOffmeshParams.agentHeight = settings->agentHeight;
    ctx->autoOffmeshParams.maxSlopeDegrees = settings->agentMaxSlope;
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API void SetAutoOffMeshGenerationParams(void* navMesh, const AutoOffmeshGenerationParams* params)
{
    if (!navMesh || !params)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->autoOffmeshParams = *params;
    ctx->autoOffmeshParams.agentRadius = ctx->genSettings.agentRadius;
    ctx->autoOffmeshParams.agentHeight = ctx->genSettings.agentHeight;
    ctx->autoOffmeshParams.maxSlopeDegrees = ctx->genSettings.agentMaxSlope;
}

GTANAVVIEWER_API void SetNavMeshCacheRoot(void* navMesh, const char* cacheRoot)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->cacheRoot = cacheRoot ? cacheRoot : "";
    ctx->dbIndexCache.clear();
    ctx->dbIndexLoaded = false;
    ctx->dbMTime = {};
}

GTANAVVIEWER_API void SetNavMeshSessionId(void* navMesh, const char* sessionId)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->sessionId = sessionId ? sessionId : "";
    ctx->dbIndexCache.clear();
    ctx->dbIndexLoaded = false;
    ctx->dbMTime = {};
}

GTANAVVIEWER_API void SetMaxResidentTiles(void* navMesh, int maxTiles)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->maxResidentTiles = std::max(1, maxTiles);
}

GTANAVVIEWER_API bool AddGeometry(void* navMesh,
                                  const char* pathToGeometry,
                                  Vector3 pos,
                                  Vector3 rot,
                                  const char* customID,
                                  bool preferBIN)
{
    if (!navMesh || !pathToGeometry || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    LoadedGeometry geom = LoadGeometry(pathToGeometry, preferBIN);
    if (!geom.Valid())
        return false;

    GeometryInstance inst{};
    inst.id = customID;
    inst.source = std::move(geom);
    inst.position = glm::vec3(pos.x, pos.y, pos.z);
    inst.rotation = glm::vec3(rot.x, rot.y, rot.z);
    UpdateWorldBounds(inst);

    auto* existing = FindGeometry(*ctx, customID);
    GeometryInstance* target = nullptr;
    if (existing)
    {
        *existing = std::move(inst);
        target = existing;
    }
    else
    {
        ctx->geometries.push_back(std::move(inst));
        target = &ctx->geometries.back();
    }

    RegisterDirtyBounds(*ctx, *target);
    ctx->rebuildAll = ctx->rebuildAll || ctx->genSettings.mode != NavmeshBuildMode::Tiled;
    return true;
}

GTANAVVIEWER_API bool UpdateGeometry(void* navMesh,
                                     const char* customID,
                                     const Vector3* pos,
                                     const Vector3* rot,
                                     bool updateNavMesh)
{
    if (!navMesh || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    auto* inst = FindGeometry(*ctx, customID);
    if (!inst)
        return false;

    glm::vec3 oldBMin = inst->worldBMin;
    glm::vec3 oldBMax = inst->worldBMax;

    if (pos)
        inst->position = glm::vec3(pos->x, pos->y, pos->z);
    if (rot)
        inst->rotation = glm::vec3(rot->x, rot->y, rot->z);

    UpdateWorldBounds(*inst);
    RegisterDirtyBounds(*ctx, *inst);

    // Also mark previous bounds to ensure tiles covering old position are refreshed
    ctx->dirtyBounds.emplace_back(oldBMin, oldBMax);

    if (updateNavMesh)
        return UpdateNavmeshState(*ctx, false);

    return true;
}

GTANAVVIEWER_API bool RemoveGeometry(void* navMesh, const char* customID)
{
    if (!navMesh || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    for (auto it = ctx->geometries.begin(); it != ctx->geometries.end(); ++it)
    {
        if (it->id == customID)
        {
            RegisterDirtyBounds(*ctx, *it);
            ctx->geometries.erase(it);
            ctx->rebuildAll = ctx->rebuildAll || ctx->genSettings.mode != NavmeshBuildMode::Tiled;
            return true;
        }
    }
    return false;
}

GTANAVVIEWER_API void RemoveAllGeometries(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->geometries.clear();
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API bool BuildNavMesh(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->rebuildAll = true;
    return UpdateNavmeshState(*ctx, true);
}

GTANAVVIEWER_API bool UpdateNavMesh(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return UpdateNavmeshState(*ctx, false);
}

GTANAVVIEWER_API bool InitTiledGrid(void* navMesh, Vector3 bmin, Vector3 bmax)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->genSettings.mode != NavmeshBuildMode::Tiled)
        return false;

    ctx->bboxMin = glm::vec3(bmin.x, bmin.y, bmin.z);
    ctx->bboxMax = glm::vec3(bmax.x, bmax.y, bmax.z);
    ctx->hasBoundingBox = true;
    ctx->streamingEnabled = true;
    ctx->rebuildAll = false;
    ctx->dirtyBounds.clear();

    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);

    float forcedMin[3] = { ctx->bboxMin.x, ctx->bboxMin.y, ctx->bboxMin.z };
    float forcedMax[3] = { ctx->bboxMax.x, ctx->bboxMax.y, ctx->bboxMax.z };
    if (!ctx->navData.InitTiledGrid(ctx->genSettings, forcedMin, forcedMax))
        return false;

    ctx->residentTiles.clear();
    ctx->residentStamp.clear();
    ctx->stampCounter = 0;
    return EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API int StreamTilesAround(void* navMesh, Vector3 center, float radius, bool allowBuildIfMissing)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav || !ctx->navData.HasTiledCache())
        return 0;

    ctx->streamingEnabled = true;
    std::filesystem::path cachePath = GetSessionCachePath(*ctx);
    const bool hasCacheFile = std::filesystem::exists(cachePath);
    const bool indexReady = hasCacheFile && EnsureDbIndexLoaded(*ctx, cachePath);

    float cachedBMin[3];
    float cachedBMax[3];
    if (!ctx->navData.GetCachedBounds(cachedBMin, cachedBMax))
        return 0;

    const glm::vec3 centerPos(center.x, center.y, center.z);
    const glm::vec3 bmin(centerPos.x - radius, cachedBMin[1], centerPos.z - radius);
    const glm::vec3 bmax(centerPos.x + radius, cachedBMax[1], centerPos.z + radius);

    std::vector<std::pair<int, int>> tiles;
    if (!ctx->navData.CollectTilesInBounds(bmin, bmax, false, tiles))
        return 0;

    std::unordered_set<uint64_t> needed;
    needed.reserve(tiles.size() * 2);
    int loadedCount = 0;

    for (const auto& tile : tiles)
    {
        const int tx = tile.first;
        const int ty = tile.second;
        const uint64_t key = MakeTileKey(tx, ty);
        needed.insert(key);

        const bool alreadyLoaded = nav->getTileRefAt(tx, ty, 0) != 0;
        if (!alreadyLoaded)
        {
            bool loaded = false;
            if (hasCacheFile && indexReady)
            {
                if (!LoadTileFromDb(cachePath.string().c_str(), nav, tx, ty, loaded, &ctx->dbIndexCache))
                {
                    printf("[NavMeshData] StreamTilesAround: falha ao carregar tile (%d,%d) do cache.\n", tx, ty);
                }
            }
            if (!loaded && allowBuildIfMissing)
            {
                std::vector<std::pair<int, int>> rebuildTiles;
                rebuildTiles.emplace_back(tx, ty);
                if (!ctx->geometries.empty())
                {
                    if (ctx->navData.RebuildSpecificTiles(rebuildTiles, ctx->genSettings, false, nullptr))
                    {
                        loaded = nav->getTileRefAt(tx, ty, 0) != 0;
                    }
                }
            }

            if (loaded)
                ++loadedCount;
        }

        if (nav->getTileRefAt(tx, ty, 0) != 0)
        {
            ctx->residentTiles.insert(key);
            ctx->residentStamp[key] = ++ctx->stampCounter;
        }
    }

    if (ctx->residentTiles.size() > static_cast<size_t>(ctx->maxResidentTiles))
    {
        while (ctx->residentTiles.size() > static_cast<size_t>(ctx->maxResidentTiles))
        {
            uint64_t lruKey = 0;
            uint32_t lruStamp = std::numeric_limits<uint32_t>::max();
            bool found = false;

            for (const auto& entry : ctx->residentTiles)
            {
                if (needed.find(entry) != needed.end())
                    continue;
                const auto itStamp = ctx->residentStamp.find(entry);
                const uint32_t stamp = itStamp != ctx->residentStamp.end() ? itStamp->second : 0;
                if (!found || stamp < lruStamp)
                {
                    found = true;
                    lruKey = entry;
                    lruStamp = stamp;
                }
            }

            if (!found)
                break;

            const int tx = static_cast<int>(lruKey >> 32);
            const int ty = static_cast<int>(lruKey & 0xffffffffu);
            const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
            if (ref != 0)
            {
                unsigned char* tileData = nullptr;
                int tileDataSize = 0;
                const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
                if (dtStatusSucceed(status) && tileData)
                    dtFree(tileData);
            }

            ctx->residentTiles.erase(lruKey);
            ctx->residentStamp.erase(lruKey);
        }
    }

    EnsureNavQuery(*ctx);
    return loadedCount;
}

GTANAVVIEWER_API void ClearAllLoadedTiles(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return;

    const int maxTiles = nav->getMaxTiles();
    for (int i = 0; i < maxTiles; ++i)
    {
        const dtMeshTile* tile = nav->getTile(i);
        if (!tile || !tile->header)
            continue;
        const dtTileRef ref = nav->getTileRef(tile);
        if (ref == 0)
            continue;
        unsigned char* tileData = nullptr;
        int tileDataSize = 0;
        const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
        if (dtStatusSucceed(status) && tileData)
            dtFree(tileData);
    }

    ctx->residentTiles.clear();
    ctx->residentStamp.clear();
    ctx->stampCounter = 0;
    EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API bool BakeTilesInBounds(void* navMesh, Vector3 bmin, Vector3 bmax, bool saveToCache)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->genSettings.mode != NavmeshBuildMode::Tiled)
        return false;

    if (ctx->geometries.empty())
        return false;

    std::vector<glm::vec3> verts;
    std::vector<unsigned int> indices;
    if (!CombineGeometry(*ctx, verts, indices))
        return false;

    if (!ctx->navData.IsLoaded() || !ctx->navData.HasTiledCache())
    {
        const glm::vec3 useMin = ctx->hasBoundingBox ? ctx->bboxMin : glm::vec3(bmin.x, bmin.y, bmin.z);
        const glm::vec3 useMax = ctx->hasBoundingBox ? ctx->bboxMax : glm::vec3(bmax.x, bmax.y, bmax.z);

        float forcedMin[3] = { useMin.x, useMin.y, useMin.z };
        float forcedMax[3] = { useMax.x, useMax.y, useMax.z };

        ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
        if (!ctx->navData.InitTiledGrid(ctx->genSettings, forcedMin, forcedMax))
            return false;

        if (!EnsureNavQuery(*ctx))
            return false;
    }

    if (!ctx->navData.UpdateCachedGeometry(verts, indices))
        return false;

    const glm::vec3 min(bmin.x, bmin.y, bmin.z);
    const glm::vec3 max(bmax.x, bmax.y, bmax.z);
    if (!ctx->navData.RebuildTilesInBounds(min, max, ctx->genSettings, false, nullptr))
        return false;

    if (saveToCache)
    {
        std::filesystem::path cachePath = GetSessionCachePath(*ctx);
        const auto& hashes = ctx->navData.GetCachedTileHashes();
        TileDbWriteOrUpdateTiles(cachePath.string().c_str(), ctx->navData.GetNavMesh(), hashes);
        ctx->dbIndexCache.clear();
        ctx->dbIndexLoaded = false;
    }

    return true;
}

GTANAVVIEWER_API int GetNavMeshPolygons(void* navMesh,
                                        NavMeshPolygonInfo* polygons,
                                        int maxPolygons,
                                        Vector3* vertices,
                                        int maxVertices,
                                        NavMeshEdgeInfo* edges,
                                        int maxEdges,
                                        int* outVertexCount,
                                        int* outEdgeCount)
{
    if (outVertexCount)
        *outVertexCount = 0;
    if (outEdgeCount)
        *outEdgeCount = 0;
    if (!navMesh || !polygons || maxPolygons <= 0 || !vertices || maxVertices <= 0 || !edges || maxEdges <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return 0;

    int writtenPolygons = 0;
    int writtenVertices = 0;
    int writtenEdges = 0;
    std::uint64_t generatedId = 1;

    const int maxTiles = nav->getMaxTiles();
    for (int tileIndex = 0; tileIndex < maxTiles && writtenPolygons < maxPolygons; ++tileIndex)
    {
        const dtMeshTile* tile = nav->getTile(tileIndex);
        if (!tile || !tile->header)
            continue;

        for (int polyIndex = 0; polyIndex < tile->header->polyCount && writtenPolygons < maxPolygons; ++polyIndex)
        {
            const dtPoly* poly = &tile->polys[polyIndex];
            if (poly->getType() != DT_POLYTYPE_GROUND)
                continue;

            const dtPolyRef pref = nav->getPolyRefBase(tile) | static_cast<unsigned int>(polyIndex);
            std::uint64_t polyId = pref != 0 ? static_cast<std::uint64_t>(pref) : generatedId++;

            const glm::vec3 center = ComputePolyCentroid(tile, poly);
            const glm::vec3 normal = ComputePolyNormal(tile, poly);

            NavMeshPolygonInfo info{};
            info.polygonId = polyId;
            info.center = ToVector3(center);
            info.normal = ToVector3(normal);
            info.vertexStart = writtenVertices;
            info.vertexCount = poly->vertCount;
            info.edgeStart = writtenEdges;
            info.edgeCount = poly->vertCount;

            if (writtenVertices + poly->vertCount > maxVertices || writtenEdges + poly->vertCount > maxEdges)
                return writtenPolygons;

            for (int v = 0; v < poly->vertCount; ++v)
            {
                const int vi = poly->verts[v];
                const float* p = &tile->verts[vi * 3];
                vertices[writtenVertices + v] = Vector3{p[0], p[1], p[2]};
            }

            for (int edge = 0; edge < poly->vertCount; ++edge)
            {
                const int vaIndex = edge;
                const int vbIndex = (edge + 1) % poly->vertCount;
                const int va = poly->verts[vaIndex];
                const int vb = poly->verts[vbIndex];
                const float* a = &tile->verts[va * 3];
                const float* b = &tile->verts[vb * 3];
                const glm::vec3 pa(a[0], a[1], a[2]);
                const glm::vec3 pb(b[0], b[1], b[2]);
                NavMeshEdgeInfo e{};
                e.vertexA = ToVector3(pa);
                e.vertexB = ToVector3(pb);
                const glm::vec3 mid = (pa + pb) * 0.5f;
                e.center = ToVector3(mid);
                const dtPolyRef neighbour = GetNeighbourRef(tile, poly, edge, nav);
                const glm::vec3 outward = ComputeEdgeOutwardNormal(pa, pb, center);
                e.normal = ToVector3(outward);
                e.polygonId = polyId;
                edges[writtenEdges + edge] = e;
            }

            writtenVertices += poly->vertCount;
            writtenEdges += poly->vertCount;
            polygons[writtenPolygons++] = info;
        }
    }

    if (outVertexCount)
        *outVertexCount = writtenVertices;
    if (outEdgeCount)
        *outEdgeCount = writtenEdges;
    return writtenPolygons;
}

GTANAVVIEWER_API int GetNavMeshBorderEdges(void* navMesh,
                                           NavMeshEdgeInfo* edges,
                                           int maxEdges,
                                           int* outEdgeCount)
{
    if (outEdgeCount)
        *outEdgeCount = 0;
    if (!navMesh || !edges || maxEdges <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return 0;

    int written = 0;
    const int maxTiles = nav->getMaxTiles();
    for (int tileIndex = 0; tileIndex < maxTiles && written < maxEdges; ++tileIndex)
    {
        const dtMeshTile* tile = nav->getTile(tileIndex);
        if (!tile || !tile->header)
            continue;

        for (int polyIndex = 0; polyIndex < tile->header->polyCount && written < maxEdges; ++polyIndex)
        {
            const dtPoly* poly = &tile->polys[polyIndex];
            if (poly->getType() != DT_POLYTYPE_GROUND)
                continue;

            const dtPolyRef pref = nav->getPolyRefBase(tile) | static_cast<unsigned int>(polyIndex);
            const glm::vec3 center = ComputePolyCentroid(tile, poly);

            for (int edge = 0; edge < poly->vertCount && written < maxEdges; ++edge)
            {
                const dtPolyRef neighbour = GetNeighbourRef(tile, poly, edge, nav);
                if (neighbour != 0)
                    continue;

                const int vaIndex = edge;
                const int vbIndex = (edge + 1) % poly->vertCount;
                const int va = poly->verts[vaIndex];
                const int vb = poly->verts[vbIndex];
                const float* a = &tile->verts[va * 3];
                const float* b = &tile->verts[vb * 3];
                const glm::vec3 pa(a[0], a[1], a[2]);
                const glm::vec3 pb(b[0], b[1], b[2]);
                NavMeshEdgeInfo e{};
                e.vertexA = ToVector3(pa);
                e.vertexB = ToVector3(pb);
                const glm::vec3 mid = (pa + pb) * 0.5f;
                e.center = ToVector3(mid);
                e.normal = ToVector3(ComputeEdgeOutwardNormal(pa, pb, center));
                e.polygonId = pref != 0 ? static_cast<std::uint64_t>(pref) : 0;
                edges[written++] = e;
            }
        }
    }

    if (outEdgeCount)
        *outEdgeCount = written;
    return written;
}

static int RunPathfindInternal(ExternNavmeshContext& ctx,
                               const glm::vec3& start,
                               const glm::vec3& end,
                               int flags,
                               int maxPoints,
                               float minEdge,
                               float* outPath, int options)
{
    if (!EnsureNavQuery(ctx))
        return 0;

    const float startPos[3] = { start.x, start.y, start.z };
    const float endPos[3]   = { end.x, end.y, end.z };

    dtQueryFilter filter{};
    filter.setIncludeFlags(static_cast<unsigned short>(flags));
    filter.setExcludeFlags(0);

    filter.setAreaCost(AREA_JUMP, 4.0f);
    filter.setAreaCost(AREA_DROP, 1.5f);
    filter.setAreaCost(AREA_OFFMESH, 2.0f);

    dtPolyRef startRef = 0, endRef = 0;
    float startNearest[3]{};
    float endNearest[3]{};

    if (dtStatusFailed(ctx.navQuery->findNearestPoly(startPos, ctx.cachedExtents, &filter, &startRef, startNearest)) || startRef == 0)
        return 0;
    if (dtStatusFailed(ctx.navQuery->findNearestPoly(endPos, ctx.cachedExtents, &filter, &endRef, endNearest)) || endRef == 0)
        return 0;

    dtPolyRef polys[256];
    int polyCount = 0;
    const dtStatus pathStatus = ctx.navQuery->findPath(startRef, endRef, startNearest, endNearest, &filter, polys, &polyCount, 256);
    if (dtStatusFailed(pathStatus) || polyCount == 0)
        return 0;

    std::vector<float> straight(static_cast<size_t>(maxPoints) * 3);
    std::vector<unsigned char> straightFlags(static_cast<size_t>(maxPoints));
    std::vector<dtPolyRef> straightRefs(static_cast<size_t>(maxPoints));
    int straightCount = 0;

    dtStatus straightStatus = DT_FAILURE;
    if (std::isfinite(minEdge) && minEdge > 0.0f)
        straightStatus = ctx.navQuery->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, options, minEdge);
    else
        straightStatus = ctx.navQuery->findStraightPath(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, options);

    if (dtStatusFailed(straightStatus) || straightCount == 0)
        return 0;

    for (int i = 0; i < straightCount; ++i)
    {
        outPath[i * 3 + 0] = straight[i * 3 + 0];
        outPath[i * 3 + 1] = straight[i * 3 + 1];
        outPath[i * 3 + 2] = straight[i * 3 + 2];
    }

    return straightCount;
}

GTANAVVIEWER_API int FindPath(void* navMesh,
                              Vector3 start,
                              Vector3 target,
                              int flags,
                              int maxPoints,
                              float* outPath, int options)
{
    if (!navMesh || !outPath || maxPoints <= 0)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return RunPathfindInternal(*ctx,
                               glm::vec3(start.x, start.y, start.z),
                               glm::vec3(target.x, target.y, target.z),
                               flags,
                               maxPoints,
                               -1.0f,
                               outPath, options);
}

GTANAVVIEWER_API int FindPathWithMinEdge(void* navMesh,
                                         Vector3 start,
                                         Vector3 target,
                                         int flags,
                                         int maxPoints,
                                         float minEdge,
                                         float* outPath, int options)
{
    if (!navMesh || !outPath || maxPoints <= 0)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return RunPathfindInternal(*ctx,
                               glm::vec3(start.x, start.y, start.z),
                               glm::vec3(target.x, target.y, target.z),
                               flags,
                               maxPoints,
                               minEdge,
                               outPath, options);
}

GTANAVVIEWER_API bool AddOffMeshLink(void* navMesh,
                                     Vector3 start,
                                     Vector3 end,
                                     bool biDir,
                                     bool updateNavMesh)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    OffmeshLink link{};
    link.start = glm::vec3(start.x, start.y, start.z);
    link.end = glm::vec3(end.x, end.y, end.z);
    link.radius = 1.0f;
    link.area = AREA_OFFMESH;
    link.flags = 1;
    link.bidirectional = biDir;
    ctx->offmeshLinks.push_back(link);
    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    ctx->rebuildAll = true;
    if (updateNavMesh)
        return UpdateNavmeshState(*ctx, false);
    return true;
}

GTANAVVIEWER_API int AddOffmeshLinksToNavMeshIsland(void* navMesh,
                                                    const IslandOffmeshLinkParams* params,
                                                    OffmeshLink* outLinks,
                                                    int maxLinks)
{
    if (!navMesh || !params || !outLinks || maxLinks <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    std::vector<OffmeshLink> generated;
    if (!ctx->navData.AddOffmeshLinksToNavMeshIsland(*params, generated))
        return 0;

    const int copyCount = std::min(maxLinks, static_cast<int>(generated.size()));
    for (int i = 0; i < copyCount; ++i)
        outLinks[i] = generated[static_cast<size_t>(i)];
    return copyCount;
}

GTANAVVIEWER_API void ClearOffMeshLinks(void* navMesh, bool updateNavMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->offmeshLinks.clear();
    ctx->navData.ClearOffmeshLinks();
    ctx->rebuildAll = true;
    if (updateNavMesh)
        UpdateNavmeshState(*ctx, false);
}

GTANAVVIEWER_API bool GenerateAutomaticOffmeshLinks(void* navMesh)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    AutoOffmeshGenerationParams params = ctx->autoOffmeshParams;
    params.agentRadius = ctx->genSettings.agentRadius;
    params.agentHeight = ctx->genSettings.agentHeight;
    params.maxSlopeDegrees = ctx->genSettings.agentMaxSlope;

    std::vector<OffmeshLink> generated;
    if (!ctx->navData.GenerateAutomaticOffmeshLinks(params, generated))
        return false;

    const unsigned int autoMask = 0xffff0000u;
    std::vector<OffmeshLink> merged;
    const auto& existing = ctx->navData.GetOffmeshLinks();
    merged.reserve(existing.size() + generated.size());
    for (const auto& link : existing)
    {
        //if ( (link.userId & autoMask) != (params.userIdBase & autoMask) )
            merged.push_back(link);
    }
    merged.insert(merged.end(), generated.begin(), generated.end());

    ctx->navData.SetOffmeshLinks(std::move(merged));
    ctx->offmeshLinks = ctx->navData.GetOffmeshLinks();
    ctx->rebuildAll = true;
    return true;
}

GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->bboxMin = glm::vec3(bmin.x, bmin.y, bmin.z);
    ctx->bboxMax = glm::vec3(bmax.x, bmax.y, bmax.z);
    ctx->hasBoundingBox = true;
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->hasBoundingBox = false;
    ctx->rebuildAll = true;
}
