#include "ViewerApp.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_set>
#include <utility>

void ViewerApp::UpdateNavmeshTiles()
{
    if (IsNavmeshJobRunning())
        return;

    if (!navmeshUpdateTiles)
        return;

    if (navGenSettings.mode != NavmeshBuildMode::Tiled)
        return;

    if (!navData.IsLoaded() || !navData.HasTiledCache())
        return;

    std::vector<std::pair<glm::vec3, glm::vec3>> dirtyBounds;
    std::unordered_map<uint64_t, MeshBoundsState> currentStates;
    currentStates.reserve(meshInstances.size());
    std::unordered_set<uint64_t> dirtyTiles;

    for (const auto& instance : meshInstances)
    {
        MeshBoundsState state = ComputeMeshBounds(instance);
        if (!state.valid)
            continue;

        currentStates[instance.id] = state;

        auto itPrev = meshStateCache.find(instance.id);
        if (itPrev == meshStateCache.end())
        {
            dirtyBounds.emplace_back(state.bmin, state.bmax);
            continue;
        }

        if (HasMeshChanged(itPrev->second, state))
        {
            glm::vec3 mergedMin = glm::min(itPrev->second.bmin, state.bmin);
            glm::vec3 mergedMax = glm::max(itPrev->second.bmax, state.bmax);
            dirtyBounds.emplace_back(mergedMin, mergedMax);
        }
    }

    for (const auto& [prevId, prevState] : meshStateCache)
    {
        if (currentStates.find(prevId) == currentStates.end() && prevState.valid)
        {
            dirtyBounds.emplace_back(prevState.bmin, prevState.bmax);
        }
    }

    meshStateCache = std::move(currentStates);

    if (dirtyBounds.empty())
        return;

    std::vector<glm::vec3> combinedVerts;
    std::vector<unsigned int> combinedIdx;
    unsigned int baseIndex = 0;
    bool hasGeometry = false;

    for (const auto& instance : meshInstances)
    {
        if (!instance.mesh)
            continue;

        glm::mat4 model = GetModelMatrix(instance);
        for (const auto& v : instance.mesh->renderVertices)
        {
            glm::vec3 world = glm::vec3(model * glm::vec4(v, 1.0f));
            combinedVerts.push_back(world);
        }

        for (auto idx : instance.mesh->indices)
        {
            combinedIdx.push_back(baseIndex + idx);
        }

        baseIndex += static_cast<unsigned int>(instance.mesh->renderVertices.size());
        hasGeometry = true;
    }

    if (!hasGeometry || combinedVerts.empty() || combinedIdx.empty())
        return;

    if (!navData.UpdateCachedGeometry(combinedVerts, combinedIdx))
        return;

    const auto tileKey = [](int tx, int ty) -> uint64_t
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
    };

    for (const auto& bounds : dirtyBounds)
    {
        std::vector<std::pair<int, int>> tilesInBounds;
        if (!navData.CollectTilesInBounds(bounds.first, bounds.second, true, tilesInBounds))
        {
            printf("[ViewerApp] UpdateNavmeshTiles: falha ao coletar tiles alteradas.\\n");
            continue;
        }

        for (const auto& tile : tilesInBounds)
        {
            dirtyTiles.insert(tileKey(tile.first, tile.second));
        }
    }

    if (dirtyTiles.empty())
        return;

    std::vector<std::pair<int, int>> tilesToRebuild;
    tilesToRebuild.reserve(dirtyTiles.size());
    for (uint64_t key : dirtyTiles)
    {
        int tx = static_cast<int>(key >> 32);
        int ty = static_cast<int>(key & 0xffffffffu);
        tilesToRebuild.emplace_back(tx, ty);
    }

    std::vector<std::pair<int, int>> touchedTiles;
    if (!navData.RebuildSpecificTiles(tilesToRebuild, navGenSettings, true, &touchedTiles))
    {
        printf("[ViewerApp] UpdateNavmeshTiles: falha ao reconstruir tiles.\\n");
        return;
    }

    if (!touchedTiles.empty())
    {
        buildNavmeshDebugLines();
        navQueryReady = false;
        if (hasPathStart && hasPathTarget)
            TryRunPathfind();
    }
}
