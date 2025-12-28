#include "ExternC.h"

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
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

    struct TempVertex { int v = 0; int vt = 0; int vn = 0; };
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
                    faceVerts.push_back(ParseFaceElement(token));

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
        return geom;
    }

    LoadedGeometry LoadGeometry(const char* path, bool preferBin)
    {
        if (!path)
            return {};

        std::filesystem::path p(path);
        if (preferBin)
        {
            std::filesystem::path binPath = p;
            binPath.replace_extension(".bin");
            if (std::filesystem::exists(binPath))
            {
                auto geom = LoadBin(binPath);
                if (geom.Valid())
                    return geom;
            }
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

        if (!ctx.navData.BuildFromMesh(verts, indices, ctx.genSettings, ctx.genSettings.mode == NavmeshBuildMode::Tiled))
            return false;

        return EnsureNavQuery(ctx);
    }

    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild)
    {
        if (forceFullBuild || ctx.rebuildAll || !ctx.navData.IsLoaded())
            return BuildNavmeshInternal(ctx);

        if (ctx.genSettings.mode != NavmeshBuildMode::Tiled || !ctx.navData.HasTiledCache())
            return BuildNavmeshInternal(ctx);

        if (!RebuildDirtyTiles(ctx))
            return BuildNavmeshInternal(ctx);

        return EnsureNavQuery(ctx);
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

static int RunPathfindInternal(ExternNavmeshContext& ctx,
                               const glm::vec3& start,
                               const glm::vec3& end,
                               int flags,
                               int maxPoints,
                               float minEdge,
                               float* outPath)
{
    if (!EnsureNavQuery(ctx))
        return 0;

    const float startPos[3] = { start.x, start.y, start.z };
    const float endPos[3]   = { end.x, end.y, end.z };

    dtQueryFilter filter{};
    filter.setIncludeFlags(static_cast<unsigned short>(flags));
    filter.setExcludeFlags(0);

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
        straightStatus = ctx.navQuery->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, 2, minEdge);
    else
        straightStatus = ctx.navQuery->findStraightPath(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, 2);

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
                              float* outPath)
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
                               outPath);
}

GTANAVVIEWER_API int FindPathWithMinEdge(void* navMesh,
                                         Vector3 start,
                                         Vector3 target,
                                         int flags,
                                         int maxPoints,
                                         float minEdge,
                                         float* outPath)
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
                               outPath);
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
    link.bidirectional = biDir;
    ctx->offmeshLinks.push_back(link);
    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    ctx->rebuildAll = true;
    if (updateNavMesh)
        return UpdateNavmeshState(*ctx, false);
    return true;
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
        if ( (link.userId & autoMask) != (params.userIdBase & autoMask) )
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
