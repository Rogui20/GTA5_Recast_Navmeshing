#pragma once
#include "GtaNavContext.h"
#include "Recast.h"
#include "DetourNavMesh.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cfloat>

class GtaNavGeometry
{
public:

    // --- estático ---
    static bool BuildStaticMergedData(NavMeshContext* ctx);

    // --- dinâmico ---
    static bool BuildDynamicMergedData(NavMeshContext* ctx);

    // --- unificação estático + dinâmico ---
    static bool RebuildCombinedGeometry(NavMeshContext* ctx);

    // --- somente dinâmico (mantém estáticos, se existirem no contexto) ---
    static bool DynamicOnly_RebuildCombinedGeometry(NavMeshContext* ctx);

    // --- pipeline simples (static + dynamic) ---
    static bool BuildMergedGeometry(NavMeshContext* ctx);
    
    static bool BuildTileGeometry(
        NavMeshContext* ctx,
        const float* tileBMin,
        const float* tileBMax,
        std::vector<float>& outVerts,
        std::vector<int>&   outTris);

private:
    static void ExpandAABB(float* bmin, float* bmax, float x, float y, float z);
};
