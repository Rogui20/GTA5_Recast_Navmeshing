#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

// Se o NavMeshDefinition NÃO estiver mais em Sample.h, pode deixar aqui.
// Se você ainda tiver uma cópia em Sample.h, comente uma delas para evitar redefinition.
struct NavMeshDefinition
{
    float navDef_m_cellSize;
    float navDef_m_cellHeight;
    float navDef_m_agentHeight;
    float navDef_m_agentRadius;
    float navDef_m_agentMaxClimb;
    float navDef_m_agentMaxSlope;
    float navDef_m_regionMinSize;
    float navDef_m_regionMergeSize;
    float navDef_m_edgeMaxLen;
    float navDef_m_edgeMaxError;
    float navDef_m_vertsPerPoly;
    float navDef_m_detailSampleDist;
    float navDef_m_detailSampleMaxError;
    int   navDef_m_partitionType;
    float navDef_m_tileSize;
};

// Mesh cache básico (props / estáticos em espaço local)
struct CachedModel
{
    std::vector<float> baseVerts; // local-space
    std::vector<int>   baseTris;
};

// Descreve uma instância de prop (modelo + transform)
struct PropInstance
{
    int         id;
    std::string modelName;
    float       pos[3]; // world-space
    float       rot[3]; // Euler ZYX em world-space
};

struct NavMeshContext
{
    // ------------------------------
    // Núcleo Recast/Detour
    // ------------------------------
    NavMeshDefinition navDef{};          // parâmetros da navmesh
    dtNavMesh*        navMesh  = nullptr;
    dtNavMeshQuery*   navQuery = nullptr;

    // Contexto de build (pode ser rcContext ou BuildContext, conforme seu projeto)
    // Se você já tem BuildContext em Sample.h, pode usar:
    //   BuildContext buildCtx;
    rcContext         buildCtx;

    // Geometria combinada que o InputGeom enxerga
    class InputGeom*  geom     = nullptr;

    // Tipo / estado
    int  navmeshType      = 0;
    bool navmeshGenerated = false;
    int  mapID            = 0;

    // ------------------------------
    // Props dinâmicos
    // ------------------------------
    std::vector<PropInstance>                props;
    int                                      nextPropID = 1;
    std::unordered_map<std::string, CachedModel> propCache;
    std::unordered_map<std::string, CachedModel> staticCache;

    // ------------------------------
    // Estáticos
    // ------------------------------
    std::vector<std::string> staticList;   // caminhos base sem extensão

    std::vector<float> staticVerts;
    std::vector<int>   staticTris;
    float staticBMin[3] = {FLT_MAX,FLT_MAX,FLT_MAX};
    float staticBMax[3] = {-FLT_MAX,-FLT_MAX,-FLT_MAX};
    bool               staticBuilt = false;

    // ------------------------------
    // Dinâmicos (props instanciados)
    // ------------------------------
    std::vector<float> dynamicVerts;
    std::vector<int>   dynamicTris;
    float dynamicBMin[3] = {FLT_MAX,FLT_MAX,FLT_MAX};
    float dynamicBMax[3] = {-FLT_MAX,-FLT_MAX,-FLT_MAX};

    // ------------------------------
    // Buffer combinado (static + dynamic)
    // ------------------------------
    std::vector<float> combinedVerts;
    std::vector<int>   combinedTris;
};


// Funções de infra básica (sem exports ainda)
NavMeshContext* GtaNav_CreateContext();
void            GtaNav_DestroyContext(NavMeshContext* ctx);

// Helpers
void GtaNav_SetNavMeshDefinition(NavMeshContext* ctx, const NavMeshDefinition& def, int navmeshType, int mapID);

// Inicializa navmesh tile-based dentro de um AABB
bool GtaNav_InitDynamicNavMesh(NavMeshContext* ctx, const float* bmin, const float* bmax);
