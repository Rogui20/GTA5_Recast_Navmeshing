#include "GtaNavProps.h"
#include <algorithm>
#include <cstdio>
#include "DetourCommon.h"


bool GtaNavProps::AddStatic(NavMeshContext* ctx, const std::string& pathBase)
{
    if (!ctx) return false;

    // Evitar duplicados
    auto it = std::find(ctx->staticList.begin(), ctx->staticList.end(), pathBase);
    if (it != ctx->staticList.end())
    {
        printf("[Props] Static '%s' já existe, ignorando.\n", pathBase.c_str());
        return true;
    }

    ctx->staticList.push_back(pathBase);
    ctx->staticBuilt = false;  // precisa rebuildar static na próxima chamada

    printf("[Props] Static adicionado: %s\n", pathBase.c_str());
    return true;
}


bool GtaNavProps::AddProp(
    NavMeshContext* ctx,
    const std::string& modelName,
    const float pos[3],
    const float rot[3])
{
    if (!ctx) return false;

    PropInstance inst;   // <-- CORRETO
    inst.id = ctx->nextPropID++;
    inst.modelName = modelName;

    inst.pos[0] = pos[0];
    inst.pos[1] = pos[1];
    inst.pos[2] = pos[2];

    inst.rot[0] = rot[0];
    inst.rot[1] = rot[1];
    inst.rot[2] = rot[2];

    ctx->props.push_back(inst);

    printf("[Props] Prop adicionado: %s  (pos=%.1f %.1f %.1f)\n",
           modelName.c_str(), pos[0], pos[1], pos[2]);

    return true;
}



// ============================================================================
// REMOVER TODOS OS ESTÁTICOS
// ============================================================================
void GtaNavProps::ClearAllStatics(NavMeshContext* ctx, bool clearCache)
{
    if (!ctx) return;

    printf("[Props] Limpando ESTÁTICOS (clearCache=%d)...\n", clearCache);

    ctx->staticList.clear();
    ctx->staticVerts.clear();
    ctx->staticTris.clear();

    ctx->staticBuilt = false;

    ctx->staticVerts.shrink_to_fit();
    ctx->staticTris.shrink_to_fit();

    if (clearCache)
        ctx->staticCache.clear();
}


// ============================================================================
// REMOVER TODOS OS PROPS DINÂMICOS
// ============================================================================
void GtaNavProps::ClearAllProps(NavMeshContext* ctx, bool clearCache)
{
    if (!ctx) return;

    printf("[Props] Limpando PROPS dinâmicos (clearCache=%d)...\n", clearCache);

    ctx->props.clear();
    ctx->props.shrink_to_fit();

    ctx->dynamicVerts.clear();
    ctx->dynamicTris.clear();
    ctx->dynamicVerts.shrink_to_fit();
    ctx->dynamicTris.shrink_to_fit();

    dtVset(ctx->dynamicBMin, 0,0,0);
    dtVset(ctx->dynamicBMax, 0,0,0);

    if (clearCache)
        ctx->propCache.clear();
}


// ============================================================================
// INSPEÇÃO
// ============================================================================
int GtaNavProps::GetStaticCount(const NavMeshContext* ctx)
{
    return ctx ? (int)ctx->staticList.size() : 0;
}

int GtaNavProps::GetPropCount(const NavMeshContext* ctx)
{
    return ctx ? (int)ctx->props.size() : 0;
}
