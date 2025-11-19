// GtaNav.cpp - Fachada principal da DLL de navmesh para GTA

#include <cstdio>
#include <cstring>

#include "GtaNavContext.h"
#include "GtaNavTiles.h"   // módulo de tiles novo (BuildTile / BuildTilesInBounds / RebuildTilesAffectedByDynamic)

// -----------------------------------------------------------------------------
// Forward declarations de funções que já existem no seu código (Sample.cpp)
// -----------------------------------------------------------------------------

// Gerenciamento de estáticos / props
extern "C" {

bool BuildStaticMergedData(NavMeshContext* ctx);
bool BuildDynamicMergedData(NavMeshContext* ctx);
bool RebuildCombinedGeometry(NavMeshContext* ctx);

bool AddStatic(NavMeshContext* ctx, const char* objPathNoExt);
bool RemoveStatic(NavMeshContext* ctx, const char* objPathNoExt, bool clearCache);
void ClearAllStatics(NavMeshContext* ctx, bool clearCache);

int  AddProp(NavMeshContext* ctx, const char* modelName, const float* pos, const float* rot);
bool RemovePropByID(NavMeshContext* ctx, int propID, bool clearCache);
void ClearAllProps(NavMeshContext* ctx, bool clearCache);

// Contexto dinâmico (tile-based)
bool InitDynamicContext(NavMeshContext* ctx, const float* bmin, const float* bmax);
bool IsDynamicContextReady(NavMeshContext* ctx);

// Limpeza geral
void FreeNavMeshResources(NavMeshContext* ctx);
}

// -----------------------------------------------------------------------------
// Helpers internos
// -----------------------------------------------------------------------------

static void ZeroVec3(float v[3])
{
    v[0] = v[1] = v[2] = 0.0f;
}

// -----------------------------------------------------------------------------
// 1) Criação / destruição de contexto
// -----------------------------------------------------------------------------
// ⚠️ REMOVIDO: essas funções já são implementadas em GtaNavContext.cpp
// e declaradas em GtaNavContext.h:
//   NavMeshContext* GtaNav_CreateContext();
//   void            GtaNav_DestroyContext(NavMeshContext* ctx);
//
// O export C (extern "C" __declspec(dllexport)) é feito por GtaNavAPI.cpp
// via GtaNav_InitNavMesh / GtaNav_FreeNavMesh.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// 2) Definições de navmesh (NavMeshDefinition)
// -----------------------------------------------------------------------------

extern "C" __declspec(dllexport)
void GtaNav_SetDefinition(NavMeshContext* ctx, const NavMeshDefinition* def)
{
    if (!ctx || !def)
        return;

    ctx->navDef = *def;

    printf("[GtaNav] NavMeshDefinition aplicada: cellSize=%.3f radius=%.3f tileSize=%.1f\n",
           def->navDef_m_cellSize,
           def->navDef_m_agentRadius,
           def->navDef_m_tileSize);
}

// -----------------------------------------------------------------------------
// 3) Estáticos (mapa estático em disco)
// -----------------------------------------------------------------------------

extern "C" __declspec(dllexport)
bool GtaNav_AddStatic(NavMeshContext* ctx, const char* basePathNoExt)
{
    if (!ctx || !basePathNoExt)
        return false;

    bool ok = AddStatic(ctx, basePathNoExt);
    if (ok)
        printf("[GtaNav] Static adicionado: %s\n", basePathNoExt);
    else
        printf("[GtaNav] ERRO ao adicionar static: %s\n", basePathNoExt);

    return ok;
}

extern "C" __declspec(dllexport)
bool GtaNav_RemoveStatic(NavMeshContext* ctx, const char* basePathNoExt, bool clearCache)
{
    if (!ctx || !basePathNoExt)
        return false;

    bool ok = RemoveStatic(ctx, basePathNoExt, clearCache);
    if (ok)
        printf("[GtaNav] Static removido: %s (clearCache=%d)\n", basePathNoExt, (int)clearCache);
    else
        printf("[GtaNav] ERRO ao remover static: %s\n", basePathNoExt);

    return ok;
}

extern "C" __declspec(dllexport)
void GtaNav_ClearStatics(NavMeshContext* ctx, bool clearCache)
{
    if (!ctx) return;

    ClearAllStatics(ctx, clearCache);
    printf("[GtaNav] Todos os estaticos limpos (clearCache=%d)\n", (int)clearCache);
}

// -----------------------------------------------------------------------------
// 4) Props dinâmicos (veículos, aviões, objetos móveis)
// -----------------------------------------------------------------------------

extern "C" __declspec(dllexport)
int GtaNav_AddProp(NavMeshContext* ctx, const char* modelNameNoExt,
                   const float* pos3, const float* rot3)
{
    if (!ctx || !modelNameNoExt || !pos3 || !rot3)
        return -1;

    int id = AddProp(ctx, modelNameNoExt, pos3, rot3);
    if (id > 0)
    {
        printf("[GtaNav] Prop adicionado: %s (id=%d)\n", modelNameNoExt, id);
    }
    else
    {
        printf("[GtaNav] ERRO ao adicionar prop: %s\n", modelNameNoExt);
    }
    return id;
}

extern "C" __declspec(dllexport)
bool GtaNav_RemoveProp(NavMeshContext* ctx, int propID, bool clearCache)
{
    if (!ctx)
        return false;

    bool ok = RemovePropByID(ctx, propID, clearCache);
    if (ok)
        printf("[GtaNav] Prop removido (id=%d, clearCache=%d)\n", propID, (int)clearCache);
    else
        printf("[GtaNav] ERRO ao remover prop (id=%d)\n", propID);

    return ok;
}

extern "C" __declspec(dllexport)
void GtaNav_ClearProps(NavMeshContext* ctx, bool clearCache)
{
    if (!ctx) return;

    ClearAllProps(ctx, clearCache);
    printf("[GtaNav] Todos os props limpos (clearCache=%d)\n", (int)clearCache);
}

// -----------------------------------------------------------------------------
// 5) Build de geometria (STATIC / DYNAMIC / COMBINED)
// -----------------------------------------------------------------------------

// Monta geometria APENAS estática, jogando em ctx->staticVerts/staticTris
extern "C" __declspec(dllexport)
bool GtaNav_BuildStaticGeometry(NavMeshContext* ctx)
{
    if (!ctx)
        return false;

    if (!BuildStaticMergedData(ctx))
    {
        printf("[GtaNav] ERRO em BuildStaticMergedData.\n");
        return false;
    }

    printf("[GtaNav] BuildStaticGeometry OK. StaticBuilt=%d, Verts=%zu Tris=%zu\n",
           (int)ctx->staticBuilt,
           ctx->staticVerts.size() / 3,
           ctx->staticTris.size() / 3);
    return true;
}

// Monta geometria APENAS dos props dinâmicos, em ctx->dynamicVerts/dynamicTris
extern "C" __declspec(dllexport)
bool GtaNav_BuildDynamicGeometry(NavMeshContext* ctx)
{
    if (!ctx)
        return false;

    if (!BuildDynamicMergedData(ctx))
    {
        printf("[GtaNav] ERRO em BuildDynamicMergedData.\n");
        return false;
    }

    printf("[GtaNav] BuildDynamicGeometry OK. Verts=%zu Tris=%zu\n",
           ctx->dynamicVerts.size() / 3,
           ctx->dynamicTris.size() / 3);
    return true;
}

// Combina STATIC + DYNAMIC (ou só um deles) em ctx->geom via initMeshFromArrays
extern "C" __declspec(dllexport)
bool GtaNav_BuildCombinedGeometry(NavMeshContext* ctx)
{
    if (!ctx)
        return false;

    if (!RebuildCombinedGeometry(ctx))
    {
        printf("[GtaNav] ERRO em RebuildCombinedGeometry.\n");
        return false;
    }

    printf("[GtaNav] BuildCombinedGeometry OK.\n");
    return true;
}

// Conveniência: build completo (static + dynamic + combined)
extern "C" __declspec(dllexport)
bool GtaNav_BuildFullGeometry(NavMeshContext* ctx)
{
    if (!ctx)
        return false;

    if (!GtaNav_BuildStaticGeometry(ctx))
        return false;

    if (!GtaNav_BuildDynamicGeometry(ctx))
        return false;

    if (!GtaNav_BuildCombinedGeometry(ctx))
        return false;

    return true;
}

// -----------------------------------------------------------------------------
// 6) Inicializar navmesh tile-based (Detour) para um AABB
// -----------------------------------------------------------------------------

extern "C" __declspec(dllexport)
bool GtaNav_InitTiledNavMesh(NavMeshContext* ctx,
                             const float* worldBMin,
                             const float* worldBMax)
{
    if (!ctx || !worldBMin || !worldBMax)
        return false;

    // Reaproveita sua função já estável, que configura dtNavMesh e dtNavMeshQuery
    if (!InitDynamicContext(ctx, worldBMin, worldBMax))
    {
        printf("[GtaNav] ERRO em InitDynamicContext.\n");
        return false;
    }

    printf("[GtaNav] InitTiledNavMesh OK.\n");
    return true;
}

// -----------------------------------------------------------------------------
// 7) Build de tiles (bounds, posição, dinâmicos)
// -----------------------------------------------------------------------------

// Build tiles em uma região do mundo (AABB em coordenadas Recast)
extern "C" __declspec(dllexport)
bool GtaNav_BuildTilesInBounds(NavMeshContext* ctx,
                               const float* bmin3,
                               const float* bmax3)
{
    if (!ctx || !bmin3 || !bmax3)
        return false;

    if (!ctx->navMesh || !ctx->geom)
    {
        printf("[GtaNav] BuildTilesInBounds: navMesh ou geom nulo.\n");
        return false;
    }

    return GtaNavTiles::BuildTilesInBounds(ctx, bmin3, bmax3);
}

// Build tiles ao redor de uma posição + raio (coordenadas Recast)
extern "C" __declspec(dllexport)
bool GtaNav_BuildTilesAroundPosition(NavMeshContext* ctx,
                                     const float* recastPos3,
                                     float radiusMeters)
{
    if (!ctx || !recastPos3)
        return false;

    if (!ctx->navMesh || !ctx->geom)
    {
        printf("[GtaNav] BuildTilesAroundPosition: navMesh ou geom nulo.\n");
        return false;
    }

    return GtaNavTiles::BuildTilesAroundPosition(ctx, recastPos3, radiusMeters);
}

// Rebuild apenas os tiles afetados pela geometria dinâmica (ctx->dynamicBMin/BMax)
extern "C" __declspec(dllexport)
bool GtaNav_RebuildTilesAffectedByDynamic(NavMeshContext* ctx)
{
    if (!ctx)
        return false;

    if (!ctx->navMesh || !ctx->geom)
    {
        printf("[GtaNav] RebuildTilesAffectedByDynamic: navMesh ou geom nulo.\n");
        return false;
    }

    return GtaNavTiles::RebuildTilesAffectedByDynamic(ctx);
}
