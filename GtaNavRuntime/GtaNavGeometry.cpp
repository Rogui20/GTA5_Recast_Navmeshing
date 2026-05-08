#include "GtaNavGeometry.h"
#include "GtaNavTransform.h"
#include "InputGeom.h"
#include "RecastAssert.h"
#include <unordered_map>
#include <cstdio>
#include <cfloat>
#include <cmath>


// helpers
void GtaNavGeometry::ExpandAABB(float* bmin, float* bmax, float x, float y, float z)
{
    bmin[0] = std::min(bmin[0], x);
    bmin[1] = std::min(bmin[1], y);
    bmin[2] = std::min(bmin[2], z);

    bmax[0] = std::max(bmax[0], x);
    bmax[1] = std::max(bmax[1], y);
    bmax[2] = std::max(bmax[2], z);
}


// ============================================================================
//  STATIC MERGED DATA
// ============================================================================
bool GtaNavGeometry::BuildStaticMergedData(NavMeshContext* ctx)
{
    if (!ctx) return false;
    if (ctx->staticList.empty())
    {
        printf("[Geom] Nenhum estático definido.\n");
        ctx->staticBuilt = false;
        return true;
    }

    printf("[Geom] BuildStaticMergedData: total=%zu\n", ctx->staticList.size());

    ctx->staticVerts.clear();
    ctx->staticTris.clear();

    float bmin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float bmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    int vertOffset = 0;

    for (auto& pathBase : ctx->staticList)
    {
        CachedModel* mdl = nullptr;

        auto it = ctx->staticCache.find(pathBase);
        if (it == ctx->staticCache.end())
        {
            rcMeshLoaderObj loader;
            std::string full = pathBase;

            if (!loader.tryLoadBIN(full))
            {
                printf("[Geom] Erro ao carregar estático: %s\n", full.c_str());
                continue;
            }

            CachedModel temp;
            temp.baseVerts.assign(loader.getVerts(),
                                  loader.getVerts() + loader.getVertCount() * 3);
            temp.baseTris.assign(loader.getTris(),
                                 loader.getTris() + loader.getTriCount() * 3);

            ctx->staticCache[pathBase] = std::move(temp);
            mdl = &ctx->staticCache[pathBase];
        }
        else mdl = &it->second;

        int vc = (int)mdl->baseVerts.size() / 3;
        int tc = (int)mdl->baseTris.size() / 3;

        // copiar vértices
        for (int i = 0; i < vc; i++)
        {
            float x = mdl->baseVerts[i*3+0];
            float y = mdl->baseVerts[i*3+1];
            float z = mdl->baseVerts[i*3+2];

            ctx->staticVerts.push_back(x);
            ctx->staticVerts.push_back(y);
            ctx->staticVerts.push_back(z);

            ExpandAABB(bmin, bmax, x, y, z);
        }

        // copiar triângulos
        for (int i = 0; i < tc; i++)
        {
            ctx->staticTris.push_back(mdl->baseTris[i*3+0] + vertOffset);
            ctx->staticTris.push_back(mdl->baseTris[i*3+1] + vertOffset);
            ctx->staticTris.push_back(mdl->baseTris[i*3+2] + vertOffset);
        }

        vertOffset += vc;
    }

    rcVcopy(ctx->staticBMin, bmin);
    rcVcopy(ctx->staticBMax, bmax);

    ctx->staticBuilt = true;

    printf("[Geom] BuildStaticMergedData OK. Verts=%zu Tris=%zu\n",
        ctx->staticVerts.size()/3, ctx->staticTris.size()/3);

    return true;
}


// ============================================================================
//  DYNAMIC MERGED DATA
// ============================================================================
bool GtaNavGeometry::BuildDynamicMergedData(NavMeshContext* ctx)
{
    if (!ctx) return false;

    ctx->dynamicVerts.clear();
    ctx->dynamicTris.clear();

    float bmin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float bmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    int vertOffset = 0;

    for (auto& inst : ctx->props)
    {
        CachedModel* mdl = nullptr;

        auto it = ctx->propCache.find(inst.modelName);
        if (it == ctx->propCache.end())
        {
            rcMeshLoaderObj loader;

            if (!loader.tryLoadBIN(inst.modelName))
            {
                printf("[Geom] (Dynamic) Erro ao carregar %s\n", inst.modelName.c_str());
                continue;
            }

            CachedModel temp;
            temp.baseVerts.assign(loader.getVerts(),
                                  loader.getVerts() + loader.getVertCount()*3);
            temp.baseTris.assign(loader.getTris(),
                                 loader.getTris() + loader.getTriCount()*3);

            ctx->propCache[inst.modelName] = std::move(temp);
            mdl = &ctx->propCache[inst.modelName];
        }
        else mdl = &it->second;

        int vc = (int)mdl->baseVerts.size() / 3;
        int tc = (int)mdl->baseTris.size() / 3;

        // transformar vértices
        for (int i = 0; i < vc; i++)
        {
            float x = mdl->baseVerts[i*3+0];
            float y = mdl->baseVerts[i*3+1];
            float z = mdl->baseVerts[i*3+2];

            float xt, yt, zt;
            TransformVertexZYX(x, y, z, inst.pos, inst.rot, xt, yt, zt);

            ctx->dynamicVerts.push_back(xt);
            ctx->dynamicVerts.push_back(yt);
            ctx->dynamicVerts.push_back(zt);

            ExpandAABB(bmin, bmax, xt, yt, zt);
        }

        for (int i = 0; i < tc; i++)
        {
            ctx->dynamicTris.push_back(mdl->baseTris[i*3+0] + vertOffset);
            ctx->dynamicTris.push_back(mdl->baseTris[i*3+1] + vertOffset);
            ctx->dynamicTris.push_back(mdl->baseTris[i*3+2] + vertOffset);
        }

        vertOffset += vc;
    }

    rcVcopy(ctx->dynamicBMin, bmin);
    rcVcopy(ctx->dynamicBMax, bmax);

    printf("[Geom] BuildDynamicMergedData OK. Verts=%zu Tris=%zu\n",
        ctx->dynamicVerts.size()/3, ctx->dynamicTris.size()/3);

    return true;
}



// ============================================================================
//  UNIFICAR STATIC + DYNAMIC
// ============================================================================
bool GtaNavGeometry::RebuildCombinedGeometry(NavMeshContext* ctx)
{
    if (!ctx) return false;

    const bool hasStatic  = ctx->staticBuilt && !ctx->staticVerts.empty();
    const bool hasDynamic = !ctx->dynamicVerts.empty();

    ctx->combinedVerts.clear();
    ctx->combinedTris.clear();

    if (hasStatic)
    {
        ctx->combinedVerts.insert(ctx->combinedVerts.end(),
                                  ctx->staticVerts.begin(),
                                  ctx->staticVerts.end());

        ctx->combinedTris.insert(ctx->combinedTris.end(),
                                 ctx->staticTris.begin(),
                                 ctx->staticTris.end());
    }

    int staticVertCount = hasStatic ? (int)(ctx->staticVerts.size() / 3) : 0;

    if (hasDynamic)
    {
        ctx->combinedVerts.insert(ctx->combinedVerts.end(),
                                  ctx->dynamicVerts.begin(),
                                  ctx->dynamicVerts.end());

        for (int t : ctx->dynamicTris)
            ctx->combinedTris.push_back(t + staticVertCount);
    }

    if (ctx->combinedVerts.empty() || ctx->combinedTris.empty())
    {
        printf("[Geom] RebuildCombinedGeometry: nenhuma geometria presente.\n");
        return false;
    }

    if (!ctx->geom)
        ctx->geom = new InputGeom();

    int totalVerts = (int)ctx->combinedVerts.size() / 3;
    int totalTris  = (int)ctx->combinedTris.size() / 3;

    bool ok = ctx->geom->initMeshFromArrays(
        &ctx->buildCtx,
        ctx->combinedVerts.data(),
        totalVerts,
        ctx->combinedTris.data(),
        totalTris
    );

    printf("[Geom] RebuildCombinedGeometry -> %s  (verts=%d tris=%d)\n",
           ok ? "OK" : "FAIL", totalVerts, totalTris);

    return ok;
}


// ============================================================================
//  SOMENTE DINÂMICO + (estático opcional)
// ============================================================================
bool GtaNavGeometry::DynamicOnly_RebuildCombinedGeometry(NavMeshContext* ctx)
{
    if (!ctx) return false;

    const bool hasStatic  = ctx->staticBuilt && !ctx->staticVerts.empty();
    const bool hasDynamic = !ctx->dynamicVerts.empty();

    ctx->combinedVerts.clear();
    ctx->combinedTris.clear();

    int staticVertCount = 0;

    if (hasStatic)
    {
        ctx->combinedVerts.insert(ctx->combinedVerts.end(),
                                  ctx->staticVerts.begin(),
                                  ctx->staticVerts.end());

        ctx->combinedTris.insert(ctx->combinedTris.end(),
                                 ctx->staticTris.begin(),
                                 ctx->staticTris.end());

        staticVertCount = (int)(ctx->staticVerts.size() / 3);
    }

    if (hasDynamic)
    {
        ctx->combinedVerts.insert(ctx->combinedVerts.end(),
                                  ctx->dynamicVerts.begin(),
                                  ctx->dynamicVerts.end());

        for (int t : ctx->dynamicTris)
            ctx->combinedTris.push_back(t + staticVertCount);
    }

    if (ctx->combinedVerts.empty() || ctx->combinedTris.empty())
    {
        printf("[Geom] DynamicOnly_RebuildCombinedGeometry: sem geometria.\n");
        return false;
    }

    if (!ctx->geom)
        ctx->geom = new InputGeom();

    int totalVerts = (int)ctx->combinedVerts.size() / 3;
    int totalTris  = (int)ctx->combinedTris.size() / 3;

    bool ok = ctx->geom->initMeshFromArrays(
        &ctx->buildCtx,
        ctx->combinedVerts.data(), totalVerts,
        ctx->combinedTris.data(), totalTris);

    printf("[Geom] DynamicOnly_RebuildCombinedGeometry -> %s\n", ok ? "OK" : "FAIL");
    return ok;
}


// ============================================================================
//  pipeline completo
// ============================================================================
bool GtaNavGeometry::BuildMergedGeometry(NavMeshContext* ctx)
{
    if (!ctx) return false;

    if (!ctx->staticBuilt && !ctx->staticList.empty())
        if (!BuildStaticMergedData(ctx))
            return false;

    if (!BuildDynamicMergedData(ctx))
        return false;

    return RebuildCombinedGeometry(ctx);
}

bool GtaNavGeometry::BuildTileGeometry(
    NavMeshContext* ctx,
    const float* tileBMin,
    const float* tileBMax,
    std::vector<float>& outVerts,
    std::vector<int>&   outTris)
{
    outVerts.clear();
    outTris.clear();

    if (!ctx || !ctx->geom)
    {
        std::printf("[GtaNavGeometry] BuildTileGeometry: ctx ou geom nulos.\n");
        return false;
    }

    const rcMeshLoaderObj* mesh = ctx->geom->getMesh();
    const rcChunkyTriMesh* chunky = ctx->geom->getChunkyMesh();

    if (!mesh || !chunky)
    {
        std::printf("[GtaNavGeometry] BuildTileGeometry: mesh ou chunkyMesh nulos.\n");
        return false;
    }

    const float* verts = mesh->getVerts();
    const int    nverts = mesh->getVertCount();

    (void)nverts; // só pra não dar warning se não usar

    // Projeção XZ do tile para consulta no ChunkyTriMesh
    float tbmin[2], tbmax[2];
    tbmin[0] = tileBMin[0];
    tbmin[1] = tileBMin[2];
    tbmax[0] = tileBMax[0];
    tbmax[1] = tileBMax[2];

    int cid[512];
    const int ncid = rcGetChunksOverlappingRect(chunky, tbmin, tbmax, cid, 512);
    if (!ncid)
    {
        // Sem triângulos nesse tile — é válido (tile vazio)
        std::printf("[GtaNavGeometry] BuildTileGeometry: nenhum chunk encontrado para tile.\n");
        return false;
    }

    outVerts.reserve(1024);
    outTris.reserve(2048);

    // Remap globalIndex -> localIndex
    std::unordered_map<int,int> vertRemap;
    vertRemap.reserve(1024);

    auto remapVertex = [&](int globalIndex) -> int
    {
        auto it = vertRemap.find(globalIndex);
        if (it != vertRemap.end())
            return it->second;

        int newIndex = static_cast<int>(outVerts.size() / 3);

        const float* v = &verts[globalIndex * 3];
        outVerts.push_back(v[0]);
        outVerts.push_back(v[1]);
        outVerts.push_back(v[2]);

        vertRemap[globalIndex] = newIndex;
        return newIndex;
    };

    // Percorre cada chunk que intersecta o tile
    for (int i = 0; i < ncid; ++i)
    {
        const rcChunkyTriMeshNode& node = chunky->nodes[cid[i]];
        const int* ctris = &chunky->tris[node.i * 3];
        const int nctris = node.n;

        for (int t = 0; t < nctris; ++t)
        {
            const int ga = ctris[t*3 + 0];
            const int gb = ctris[t*3 + 1];
            const int gc = ctris[t*3 + 2];

            // Remap para índices locais
            const int la = remapVertex(ga);
            const int lb = remapVertex(gb);
            const int lc = remapVertex(gc);

            outTris.push_back(la);
            outTris.push_back(lb);
            outTris.push_back(lc);
        }
    }

    std::printf("[GtaNavGeometry] BuildTileGeometry OK. Verts=%zu Tris=%zu\n",
        outVerts.size() / 3, outTris.size() / 3);

    return !outTris.empty();
}