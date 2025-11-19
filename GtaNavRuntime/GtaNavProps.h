#pragma once
#include "GtaNavContext.h"
#include <string>
#include <vector>

class GtaNavProps
{
public:

    //
    // --- ADICIONAR ESTÁTICOS ---
    //
    // pathBase → caminho sem extensão (ex: "C:/GTA/Static/airport_01")
    //
    static bool AddStatic(NavMeshContext* ctx, const std::string& pathBase);

    //
    // --- ADICIONAR PROP DINÂMICO ---
    //
    static bool AddProp(
        NavMeshContext* ctx,
        const std::string& modelName,
        const float pos[3],
        const float rot[3]
    );

    //
    // --- REMOÇÃO ---
    //
    static void ClearAllStatics(NavMeshContext* ctx, bool clearCache = false);
    static void ClearAllProps(NavMeshContext* ctx, bool clearCache = false);

    //
    // --- INSPEÇÃO ---
    //
    static int GetStaticCount(const NavMeshContext* ctx);
    static int GetPropCount(const NavMeshContext* ctx);
};
