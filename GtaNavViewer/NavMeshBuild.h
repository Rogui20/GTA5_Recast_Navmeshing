#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>
#include <Recast.h>
#include <DetourNavMesh.h>

#include "NavMeshData.h"

struct NavmeshBuildInput
{
    rcContext& ctx;
    const std::vector<float>& verts;
    const std::vector<int>& tris;
    int nverts = 0;
    int ntris = 0;
    float meshBMin[3] = {};
    float meshBMax[3] = {};
    rcConfig baseCfg{};
    const std::vector<OffmeshLink>* offmeshLinks = nullptr;
};

bool BuildSingleNavMesh(const NavmeshBuildInput& input,
                        const NavmeshGenerationSettings& settings,
                        dtNavMesh*& outNav);

bool BuildTiledNavMesh(const NavmeshBuildInput& input,
                       const NavmeshGenerationSettings& settings,
                       dtNavMesh*& outNav,
                       bool buildTilesNow = true,
                       int* outTilesBuilt = nullptr,
                       int* outTilesSkipped = nullptr,
                       const std::atomic_bool* cancelFlag = nullptr,
                       bool useCache = true,
                       const char* cachePath = nullptr,
                       std::unordered_map<uint64_t, uint64_t>* outTileHashes = nullptr);

bool BuildSingleTile(const NavmeshBuildInput& input,
                     const NavmeshGenerationSettings& settings,
                     int tileX,
                     int tileY,
                     dtNavMesh* nav,
                     bool& outBuilt,
                     bool& outEmpty);