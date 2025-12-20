// ViewerApp.cpp
#include <imgui.h>
#include "ViewerApp.h"
#include "ViewerCamera.h"
#include "RendererGL.h"
#include "ObjLoader.h"
#include "Mesh.h"
#include "RenderMode.h"
#include "NavMeshData.h"
#include "GtaNavAPI.h"

#include <glad/glad.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/component_wise.hpp>
#include <limits>
#include <glm/geometric.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>
#include <array>
#include <cmath>
#include <cstdio>


ViewerApp::ViewerApp()
    : directoryBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
    , propHashBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
    , objDirectoryBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
{
    currentDirectory = std::filesystem::current_path().string();
    directoryBrowser.SetTitle("Selecionar pasta");
    directoryBrowser.SetDirectory(currentDirectory);
    propHashBrowser.SetTitle("Selecionar PropHashToName.json");
    propHashBrowser.SetTypeFilters({".json"});
    objDirectoryBrowser.SetTitle("Selecionar pasta OBJ (Memory Handler)");
    for (auto& idCounter : nextMeshIdSlots)
        idCounter = 1;
    for (int i = 0; i < kMaxNavmeshSlots; ++i)
    {
        geometrySlotForNavSlot[i] = i;
    }
    for (auto& picked : pickedMeshIndexSlots)
        picked = -1;
    for (auto& pickedTri : pickedTriSlots)
        pickedTri = -1;

    if (!memoryHandler.GetPropHashFile().empty())
    {
        propHashBrowser.SetPwd(memoryHandler.GetPropHashFile().parent_path());
    }
    if (!memoryHandler.GetObjDirectory().empty())
    {
        objDirectoryBrowser.SetPwd(memoryHandler.GetObjDirectory());
    }
}

ViewerApp::~ViewerApp()
{
    memoryHandler.SetMonitoringEnabled(false);
    for (auto& query : navQuerySlots)
    {
        if (query)
        {
            dtFreeNavMeshQuery(query);
            query = nullptr;
        }
    }
}

void ViewerApp::ClearNavmeshSlotData(int slotIndex)
{
    if (!IsValidSlot(slotIndex))
        return;

    if (navQuerySlots[slotIndex])
    {
        dtFreeNavMeshQuery(navQuerySlots[slotIndex]);
        navQuerySlots[slotIndex] = nullptr;
    }

    navmeshDataSlots[slotIndex] = {};
    navMeshTrisSlots[slotIndex].clear();
    navMeshLinesSlots[slotIndex].clear();
    navmeshLineBufferSlots[slotIndex].clear();
    offmeshLinkLinesSlots[slotIndex].clear();
    pathLinesSlots[slotIndex].clear();
    navQueryReadySlots[slotIndex] = false;
    hasPathStartSlots[slotIndex] = false;
    hasPathTargetSlots[slotIndex] = false;
    hasOffmeshStartSlots[slotIndex] = false;
    hasOffmeshTargetSlots[slotIndex] = false;
    offmeshStartSlots[slotIndex] = {};
    offmeshTargetSlots[slotIndex] = {};
    meshInstancesSlots[slotIndex].clear();
    meshStateCacheSlots[slotIndex].clear();
    nextMeshIdSlots[slotIndex] = 1;
    pickedMeshIndexSlots[slotIndex] = -1;
    pickedTriSlots[slotIndex] = -1;
    geometrySlotForNavSlot[slotIndex] = slotIndex;
}

void ViewerApp::ResetAllNavmeshSlots()
{
    for (int slot = 0; slot < kMaxNavmeshSlots; ++slot)
    {
        ClearNavmeshSlotData(slot);
        geometrySlotForNavSlot[slot] = slot;
    }
    currentNavmeshSlot = 0;
    ResetPathState(currentNavmeshSlot);
}

void ViewerApp::SwitchNavmeshSlot(int slotIndex)
{
    if (!IsValidSlot(slotIndex) || slotIndex == currentNavmeshSlot)
        return;

    const int previousNavSlot = currentNavmeshSlot;
    currentNavmeshSlot = slotIndex;
    int desiredGeometrySlot = geometrySlotForNavSlot[slotIndex];
    if (!IsValidSlot(desiredGeometrySlot))
        desiredGeometrySlot = slotIndex;
    if (currentGeometrySlot == previousNavSlot || !IsValidSlot(currentGeometrySlot))
        currentGeometrySlot = desiredGeometrySlot;
}

bool ViewerApp::buildNavmeshFromMeshes(bool buildTilesNow, int slotIndex)
{
    if (IsNavmeshJobRunning())
    {
        navmeshJobQueued = true;
        navmeshJobQueuedBuildTilesNow = navmeshJobQueuedBuildTilesNow || buildTilesNow;
        navmeshJobQueuedSlot = slotIndex >= 0 && IsValidSlot(slotIndex) ? slotIndex : currentNavmeshSlot;
        printf("[ViewerApp] buildNavmeshFromMeshes: já existe um build em andamento. Marcando rebuild pendente...\n");
        return false;
    }

    const int targetSlot = slotIndex >= 0 && IsValidSlot(slotIndex) ? slotIndex : currentNavmeshSlot;

    if (CurrentMeshes().empty())
    {
        printf("[ViewerApp] buildNavmeshFromMeshes: sem meshes carregadas.\n");
        navMeshTrisSlots[targetSlot].clear();
        navMeshLinesSlots[targetSlot].clear();
        navmeshLineBufferSlots[targetSlot].clear();
        ResetPathState(targetSlot);
        navQueryReadySlots[targetSlot] = false;
        return false;
    }

    std::vector<glm::vec3> combinedVerts;
    std::vector<unsigned int> combinedIdx;
    unsigned int baseIndex = 0;

    for (const auto& instance : CurrentMeshes())
    {
        if (!instance.mesh)
            continue;

        glm::mat4 model = GetModelMatrix(instance);
        for (const auto& v : instance.mesh->renderVertices)
        {
            combinedVerts.push_back(glm::vec3(model * glm::vec4(v, 1.0f)));
        }

        for (auto idx : instance.mesh->indices)
        {
            combinedIdx.push_back(baseIndex + idx);
        }

        baseIndex += static_cast<unsigned int>(instance.mesh->renderVertices.size());
    }

    editDragActive = false;
    activeGizmoAxis = GizmoAxis::None;

    JoinNavmeshWorker();
    navmeshJobResult.reset();
    navmeshJobCompleted.store(false);
    navmeshJobRunning.store(true);
    navmeshCancelRequested.store(false);
    navmeshJobQueued = false;
    navmeshJobQueuedBuildTilesNow = buildTilesNow;
    navmeshJobQueuedSlot = -1;

    const auto settingsCopy = navGenSettings;
    const auto offmeshLinksCopy = navmeshDataSlots[targetSlot].GetOffmeshLinks();

    navmeshWorker = std::thread([
        this,
        verts = std::move(combinedVerts),
        idx = std::move(combinedIdx),
        settingsCopy,
        buildTilesNow,
        offmeshLinksCopy,
        targetSlot
    ]() mutable
    {
        auto result = std::make_unique<NavmeshJobResult>();
        result->slotIndex = targetSlot;
        result->navData.SetOffmeshLinks(offmeshLinksCopy);
        if (!verts.empty() && !idx.empty())
        {
            result->success = result->navData.BuildFromMesh(verts, idx, settingsCopy, buildTilesNow, &navmeshCancelRequested);
        }

        if (result->success)
        {
            result->navData.ExtractDebugMesh(result->tris, result->lines);
            printf("[ViewerApp] Navmesh build finalizado em worker.\n");
        }
        else
        {
            printf("[ViewerApp] BuildFromMesh falhou no worker.\n");
        }

        {
            std::lock_guard<std::mutex> lock(navmeshJobMutex);
            navmeshJobResult = std::move(result);
        }

        navmeshJobRunning.store(false);
        navmeshJobCompleted.store(true);
    });

    return true;
}

void ViewerApp::buildNavmeshDebugLines(int slotIndex)
{
    const int slot = IsValidSlot(slotIndex) ? slotIndex : currentNavmeshSlot;
    auto& tris = navMeshTrisSlots[slot];
    auto& lines = navMeshLinesSlots[slot];
    auto& buffer = navmeshLineBufferSlots[slot];

    tris.clear();
    lines.clear();
    buffer.clear();

    navmeshDataSlots[slot].ExtractDebugMesh(tris, lines);
    RebuildDebugLineBuffer(slot);
}

void ViewerApp::RebuildDebugLineBuffer(int slotIndex)
{
    const int slot = IsValidSlot(slotIndex) ? slotIndex : currentNavmeshSlot;
    auto& lines = navMeshLinesSlots[slot];
    auto& buffer = navmeshLineBufferSlots[slot];
    buffer.clear();

    for (size_t i = 0; i + 1 < lines.size(); i += 2)
    {
        DebugLine dl;
        dl.x0 = lines[i].x;
        dl.y0 = lines[i].y;
        dl.z0 = lines[i].z;
        dl.x1 = lines[i+1].x;
        dl.y1 = lines[i+1].y;
        dl.z1 = lines[i+1].z;
        buffer.push_back(dl);
    }

    RebuildOffmeshLinkLines(slot);

    printf("[ViewerApp] Linhas de navmesh: %zu segmentos. Offmesh links: %zu.\n",
           buffer.size(), offmeshLinkLinesSlots[slot].size());
}

void ViewerApp::RebuildOffmeshLinkLines(int slotIndex)
{
    const int slot = IsValidSlot(slotIndex) ? slotIndex : currentNavmeshSlot;
    auto& offmeshLines = offmeshLinkLinesSlots[slot];
    offmeshLines.clear();

    const auto& offmeshLinks = navmeshDataSlots[slot].GetOffmeshLinks();
    for (const auto& link : offmeshLinks)
    {
        DebugLine dl{};
        dl.x0 = link.start.x;
        dl.y0 = link.start.y;
        dl.z0 = link.start.z;
        dl.x1 = link.end.x;
        dl.y1 = link.end.y;
        dl.z1 = link.end.z;
        offmeshLines.push_back(dl);
    }
}

bool ViewerApp::BuildStaticMap(GtaHandler& handler, const std::filesystem::path& meshDirectory, float scanRange)
{
    if (!camera)
    {
        printf("[ViewerApp] BuildStaticMap falhou: câmera inválida.\n");
        return false;
    }

    if (IsNavmeshJobRunning())
    {
        printf("[ViewerApp] BuildStaticMap adiado: navmesh está sendo gerado.\n");
        return false;
    }

    glm::vec3 gtaPos = ToGtaCoords(camera->pos);
    auto sources = handler.LookupSources(gtaPos, scanRange);

    if (sources.empty())
    {
        printf("[ViewerApp] BuildStaticMap: nenhuma instância encontrada no raio %.2f.\n", scanRange);
        return false;
    }

    size_t loaded = 0;
    for (const auto& source : sources)
    {
        std::filesystem::path fullPath = meshDirectory / source;
        if (IsMeshAlreadyLoaded(fullPath))
        {
            continue;
        }

        if (!std::filesystem::exists(fullPath))
        {
            printf("[ViewerApp] BuildStaticMap: arquivo não encontrado %s.\n", fullPath.string().c_str());
            continue;
        }

        if (LoadMeshFromPathWithOptions(fullPath.string(), false, true))
        {
            ++loaded;
        }
    }

    printf("[ViewerApp] BuildStaticMap: %zu/%zu meshes carregadas.\n", loaded, sources.size());
    return loaded > 0;
}

void ViewerApp::SetProceduralTestEnabled(bool enabled)
{
    proceduralTestEnabled = enabled;
    proceduralHasLastScan = false;
    proceduralBuiltTiles.clear();
    proceduralNavmeshPending = false;
    proceduralNavmeshNeedsRebuild = enabled;
}

bool ViewerApp::PrepareProceduralNavmeshIfNeeded()
{
    if (IsNavmeshJobRunning())
        return false;

    if (proceduralNavmeshPending)
    {
        if (CurrentNavData().IsLoaded() && CurrentNavData().HasTiledCache())
        {
            proceduralNavmeshPending = false;
            return true;
        }

        proceduralNavmeshNeedsRebuild = true;
        proceduralNavmeshPending = false;
    }

    if (!proceduralNavmeshNeedsRebuild)
        return CurrentNavData().IsLoaded() && CurrentNavData().HasTiledCache();

    if (navGenSettings.mode != NavmeshBuildMode::Tiled)
    {
        printf("[ViewerApp] Procedural Test: forçando modo tiled para cache incremental.\n");
        navGenSettings.mode = NavmeshBuildMode::Tiled;
    }

    if (CurrentMeshes().empty())
        return false;

    if (buildNavmeshFromMeshes(false))
    {
        proceduralNavmeshPending = true;
        proceduralNavmeshNeedsRebuild = false;
    }

    return false;
}

void ViewerApp::RunProceduralTestStep()
{
    if (!proceduralTestEnabled)
        return;

    if (!camera)
        return;

    const float scanRange = gtaHandlerMenu.GetScanRange();
    const auto& meshDir = gtaHandlerMenu.GetMeshDirectory();

    if (meshDir.empty() || scanRange <= 0.0f || !gtaHandler.HasCache())
        return;

    glm::vec3 gtaPos = ToGtaCoords(camera->pos);
    glm::vec3 delta = gtaPos - proceduralLastScanGtaPos;
    float distSq = glm::dot(delta, delta);
    float thresholdSq = scanRange * scanRange;

    if (!proceduralHasLastScan || distSq >= thresholdSq)
    {
        if (BuildStaticMap(gtaHandler, meshDir, scanRange))
        {
            proceduralLastScanGtaPos = gtaPos;
            proceduralHasLastScan = true;
            proceduralBuiltTiles.clear();
            proceduralNavmeshNeedsRebuild = true;
            proceduralNavmeshPending = false;
        }
    }

    if (!PrepareProceduralNavmeshIfNeeded())
        return;

    BuildProceduralTilesAroundCamera();
}

void ViewerApp::BuildProceduralTilesAroundCamera()
{
    if (!CurrentNavData().IsLoaded() || !CurrentNavData().HasTiledCache())
        return;

    if (IsNavmeshJobRunning())
        return;

    const float tileWorldSize = navGenSettings.tileSize * navGenSettings.cellSize;
    if (tileWorldSize <= 0.0f)
        return;

    int gridLayers = std::max(1, gtaHandlerMenu.GetProceduralTileGridLayers());
    float halfExtentScale = std::max(0.001f, static_cast<float>(gridLayers - 1));

    glm::vec3 center = camera->pos;
    glm::vec3 halfExtents(tileWorldSize * halfExtentScale, tileWorldSize * halfExtentScale, tileWorldSize * halfExtentScale);

    glm::vec3 bmin = center - halfExtents;
    glm::vec3 bmax = center + halfExtents;

    std::vector<std::pair<int, int>> tiles;
    if (!CurrentNavData().CollectTilesInBounds(bmin, bmax, false, tiles))
        return;

    const auto tileKey = [](int tx, int ty) -> uint64_t
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
    };

    std::vector<std::pair<int, int>> tilesToBuild;
    tilesToBuild.reserve(tiles.size());

    for (const auto& tile : tiles)
    {
        uint64_t key = tileKey(tile.first, tile.second);
        if (proceduralBuiltTiles.find(key) == proceduralBuiltTiles.end())
        {
            tilesToBuild.push_back(tile);
        }
    }

    if (tilesToBuild.empty())
        return;

    const size_t maxTilesPerStep = 512;
    if (tilesToBuild.size() > maxTilesPerStep)
    {
        printf("[ViewerApp] Procedural: limitando rebuild para %zu tiles (solicitados=%zu).\n", maxTilesPerStep, tilesToBuild.size());
        tilesToBuild.resize(maxTilesPerStep);
    }

    std::vector<std::pair<int, int>> touchedTiles;
    if (!CurrentNavData().RebuildSpecificTiles(tilesToBuild, navGenSettings, false, &touchedTiles))
        return;

    for (const auto& tile : tilesToBuild)
    {
        proceduralBuiltTiles.insert(tileKey(tile.first, tile.second));
    }

    if (!touchedTiles.empty())
    {
        buildNavmeshDebugLines();
        CurrentNavQueryReady() = false;
        if (CurrentHasPathStart() && CurrentHasPathTarget())
            TryRunPathfind();
    }
}

void ViewerApp::DrawSelectedMeshHighlight()
{
    if (viewportClickMode != ViewportClickMode::EditMesh)
        return;

    if (!drawMeshOutlines)
        return;

    if (CurrentPickedMeshIndex() < 0 || CurrentPickedMeshIndex() >= static_cast<int>(CurrentMeshes().size()))
        return;

    const auto& instance = CurrentMeshes()[CurrentPickedMeshIndex()];
    if (!instance.mesh)
        return;

    glm::mat4 model = GetModelMatrix(instance);

    std::unordered_set<uint64_t> edgeSet;
    std::vector<DebugLine> outlines;
    outlines.reserve(instance.mesh->indices.size());

    auto addEdge = [&](int ia, int ib)
    {
        int minIdx = std::min(ia, ib);
        int maxIdx = std::max(ia, ib);
        uint64_t key = (static_cast<uint64_t>(minIdx) << 32) | static_cast<uint32_t>(maxIdx);
        if (edgeSet.insert(key).second)
        {
            glm::vec3 a = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[ia], 1.0f));
            glm::vec3 b = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[ib], 1.0f));

            DebugLine l{a.x, a.y, a.z, b.x, b.y, b.z};
            outlines.push_back(l);
        }
    };

    for (size_t i = 0; i + 2 < instance.mesh->indices.size(); i += 3)
    {
        int i0 = instance.mesh->indices[i+0];
        int i1 = instance.mesh->indices[i+1];
        int i2 = instance.mesh->indices[i+2];

        glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
        glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
        glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

        addEdge(i0, i1);
        addEdge(i1, i2);
        addEdge(i2, i0);
    }

    if (!outlines.empty())
    {
        glDisable(GL_DEPTH_TEST);
        renderer->drawNavmeshLines(outlines, glm::vec3(1.0f, 0.5f, 0.0f), 3.0f);
        glEnable(GL_DEPTH_TEST);
    }
}

void ViewerApp::DrawEditGizmo()
{
    if (viewportClickMode != ViewportClickMode::EditMesh)
        return;

    if (CurrentPickedMeshIndex() < 0 || CurrentPickedMeshIndex() >= static_cast<int>(CurrentMeshes().size()))
        return;

    const auto& instance = CurrentMeshes()[CurrentPickedMeshIndex()];
    glm::vec3 origin = instance.position;
    float camDist = glm::length(camera->pos - origin);
    float axisLength = 4.0f + camDist * 0.05f;
    float lineWidth = 5.0f;

    glm::vec3 axisX = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::X) : glm::vec3(1,0,0);
    glm::vec3 axisY = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::Y) : glm::vec3(0,1,0);
    glm::vec3 axisZ = moveTransformMode == MoveTransformMode::LocalTransform ? GetAxisDirection(instance, GizmoAxis::Z) : glm::vec3(0,0,1);

    auto gizmoColor = [&](GizmoAxis axis, const glm::vec3& baseColor)
    {
        if (activeGizmoAxis == axis)
            return baseColor * 1.2f;
        return baseColor;
    };

    if (meshEditMode == MeshEditMode::Move)
    {
        std::array<std::pair<GizmoAxis, glm::vec3>, 3> axisInfo = {
            std::make_pair(GizmoAxis::X, axisX),
            std::make_pair(GizmoAxis::Y, axisY),
            std::make_pair(GizmoAxis::Z, axisZ)
        };

        for (const auto& axis : axisInfo)
        {
            glm::vec3 dir = glm::normalize(axis.second);
            DebugLine line{origin.x, origin.y, origin.z,
                           origin.x + dir.x * axisLength,
                           origin.y + dir.y * axisLength,
                           origin.z + dir.z * axisLength};

            glm::vec3 color(1,0,0);
            if (axis.first == GizmoAxis::Y) color = glm::vec3(0,1,0);
            else if (axis.first == GizmoAxis::Z) color = glm::vec3(0,0,1);

            renderer->drawNavmeshLines({line}, gizmoColor(axis.first, color), lineWidth);
        }
    }

    if (meshEditMode == MeshEditMode::Rotate)
    {
        float radius = 3.5f + camDist * 0.03f;
        const int segments = 64;

    auto drawCircle = [&](GizmoAxis axis, const glm::vec3& basisA, const glm::vec3& basisB, const glm::vec3& color)
    {
        std::vector<DebugLine> circle;
        circle.reserve(segments);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = (static_cast<float>(i) / segments) * glm::two_pi<float>();
            float a1 = (static_cast<float>(i + 1) / segments) * glm::two_pi<float>();

            glm::vec3 p0 = origin + basisA * std::cos(a0) * radius + basisB * std::sin(a0) * radius;
            glm::vec3 p1 = origin + basisA * std::cos(a1) * radius + basisB * std::sin(a1) * radius;

            circle.push_back({p0.x, p0.y, p0.z, p1.x, p1.y, p1.z});
        }

        renderer->drawNavmeshLines(circle, gizmoColor(axis, color), 3.0f);
    };

        glm::vec3 right = GetAxisDirection(instance, GizmoAxis::X);
        glm::vec3 up    = GetAxisDirection(instance, GizmoAxis::Y);
        glm::vec3 fwd   = GetAxisDirection(instance, GizmoAxis::Z);

        drawCircle(GizmoAxis::X, up, fwd, glm::vec3(1,0,0));
        drawCircle(GizmoAxis::Y, right, fwd, glm::vec3(0,1,0));
        drawCircle(GizmoAxis::Z, glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(0,0,1));
    }
}

void ViewerApp::ClearBoundingBox()
{
    boundingBoxVisible = false;
    boundingBoxAwaitingSecondClick = false;
    boundingBoxLines.clear();
    boundingBoxFill.clear();
    boundingBoxActiveAxis = GizmoAxis::None;
    boundingBoxDragActive = false;
}

void ViewerApp::RebuildBoundingBoxDebug()
{
    if (!boundingBoxVisible)
        return;

    boundingBoxMin = glm::min(boundingBoxP0, boundingBoxP1);
    boundingBoxMax = glm::max(boundingBoxP0, boundingBoxP1);

    const float eps = 0.01f;
    if (glm::all(glm::lessThan(glm::abs(boundingBoxMax - boundingBoxMin), glm::vec3(eps))))
    {
        boundingBoxMax = boundingBoxMin + glm::vec3(eps);
    }

    boundingBoxLines.clear();
    boundingBoxFill.clear();

    glm::vec3 corners[8] = {
        {boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z},
        {boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z},
        {boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z},
        {boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z},
        {boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z},
        {boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z},
        {boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z},
        {boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z}
    };

    auto addLine = [&](int a, int b)
    {
        boundingBoxLines.push_back({corners[a].x, corners[a].y, corners[a].z,
                                    corners[b].x, corners[b].y, corners[b].z});
    };

    addLine(0,1); addLine(1,2); addLine(2,3); addLine(3,0);
    addLine(4,5); addLine(5,6); addLine(6,7); addLine(7,4);
    addLine(0,4); addLine(1,5); addLine(2,6); addLine(3,7);

    auto addFace = [&](int a, int b, int c, int d)
    {
        boundingBoxFill.push_back(corners[a]);
        boundingBoxFill.push_back(corners[b]);
        boundingBoxFill.push_back(corners[c]);

        boundingBoxFill.push_back(corners[a]);
        boundingBoxFill.push_back(corners[c]);
        boundingBoxFill.push_back(corners[d]);
    };

    addFace(0,1,2,3); // bottom
    addFace(4,5,6,7); // top
    addFace(0,1,5,4); // front
    addFace(1,2,6,5); // right
    addFace(2,3,7,6); // back
    addFace(3,0,4,7); // left
}

bool ViewerApp::BeginBoundingBoxDrag(const Ray& ray)
{
    if (!boundingBoxVisible)
        return false;

    glm::vec3 center = (boundingBoxMin + boundingBoxMax) * 0.5f;
    glm::vec3 extents = (boundingBoxMax - boundingBoxMin) * 0.5f;
    const float minExtent = std::max(0.1f, glm::compMax(extents));

    struct Candidate
    {
        float distance = std::numeric_limits<float>::max();
        GizmoAxis axis = GizmoAxis::None;
        int sign = 1;
        float axisParam = 0.0f;
    } best;

    auto testAxis = [&](GizmoAxis axis, int sign, const glm::vec3& dir, float extent)
    {
        glm::vec3 origin = center;
        float axisParam = 0.0f;
        float rayParam = 0.0f;

        const float EPS = 1e-5f;
        float a = glm::dot(dir, dir);
        float b = glm::dot(dir, ray.dir);
        float c = glm::dot(ray.dir, ray.dir);
        glm::vec3 w0 = origin - ray.origin;
        float d = glm::dot(dir, w0);
        float e = glm::dot(ray.dir, w0);
        float denom = a * c - b * b;
        if (std::fabs(denom) < EPS)
            return;

        axisParam = (b * e - c * d) / denom;
        rayParam  = (a * e - b * d) / denom;
        if (rayParam < 0.0f)
            return;
        if (axisParam < -extent || axisParam > extent)
            return;

        glm::vec3 pAxis = origin + dir * axisParam;
        glm::vec3 pRay  = ray.origin + ray.dir * rayParam;
        float dist = glm::length(pAxis - pRay);
        float tolerance = 0.5f + minExtent * 0.1f;
        if (dist > tolerance)
            return;

        if (dist < best.distance)
        {
            best.distance = dist;
            best.axis = axis;
            best.sign = sign;
            best.axisParam = axisParam;
        }
    };

    testAxis(GizmoAxis::X, +1, glm::vec3(1,0,0), extents.x);
    testAxis(GizmoAxis::X, -1, glm::vec3(-1,0,0), extents.x);
    testAxis(GizmoAxis::Y, +1, glm::vec3(0,1,0), extents.y);
    testAxis(GizmoAxis::Y, -1, glm::vec3(0,-1,0), extents.y);
    testAxis(GizmoAxis::Z, +1, glm::vec3(0,0,1), extents.z);
    testAxis(GizmoAxis::Z, -1, glm::vec3(0,0,-1), extents.z);

    if (best.axis == GizmoAxis::None)
        return false;

    boundingBoxActiveAxis = best.axis;
    boundingBoxDragSign = best.sign;
    boundingBoxGizmoDir = glm::normalize(glm::vec3(
        best.axis == GizmoAxis::X ? 1.0f * best.sign : 0.0f,
        best.axis == GizmoAxis::Y ? 1.0f * best.sign : 0.0f,
        best.axis == GizmoAxis::Z ? 1.0f * best.sign : 0.0f));
    boundingBoxStartMin = boundingBoxMin;
    boundingBoxStartMax = boundingBoxMax;
    boundingBoxStartParam = best.axisParam;
    boundingBoxDragActive = true;
    return true;
}

void ViewerApp::UpdateBoundingBoxDrag(int mouseX, int mouseY)
{
    if (!boundingBoxDragActive || boundingBoxActiveAxis == GizmoAxis::None)
        return;

    int screenW = 1600;
    int screenH = 900;
    SDL_GetWindowSize(window, &screenW, &screenH);
    Ray ray = camera->GetRayFromScreen(mouseX, mouseY, screenW, screenH);

    glm::vec3 center = (boundingBoxStartMin + boundingBoxStartMax) * 0.5f;
    glm::vec3 dir = boundingBoxGizmoDir;
    float a = glm::dot(dir, dir);
    float b = glm::dot(dir, ray.dir);
    float c = glm::dot(ray.dir, ray.dir);
    glm::vec3 w0 = center - ray.origin;
    float d = glm::dot(dir, w0);
    float e = glm::dot(ray.dir, w0);
    float denom = a * c - b * b;
    if (std::fabs(denom) < 1e-6f)
        return;

    float axisParam = (b * e - c * d) / denom;
    float rayParam  = (a * e - b * d) / denom;
    if (rayParam < 0.0f)
        return;

    float delta = axisParam - boundingBoxStartParam;
    glm::vec3 offset = dir * delta;

    boundingBoxMin = boundingBoxStartMin;
    boundingBoxMax = boundingBoxStartMax;
    if (boundingBoxDragSign > 0)
        boundingBoxMax += offset;
    else
        boundingBoxMin += offset;

    boundingBoxP0 = boundingBoxMin;
    boundingBoxP1 = boundingBoxMax;
    RebuildBoundingBoxDebug();
}

void ViewerApp::DrawBoundingBoxGizmo()
{
    if (!boundingBoxVisible)
        return;

    glm::vec3 center = (boundingBoxMin + boundingBoxMax) * 0.5f;
    glm::vec3 extents = (boundingBoxMax - boundingBoxMin) * 0.5f;
    std::vector<DebugLine> gizmoLines;
    gizmoLines.reserve(6);

    auto addHandle = [&](GizmoAxis axis, const glm::vec3& dir, float extent)
    {
        glm::vec3 endPos = center + dir * extent;
        gizmoLines.push_back({center.x, center.y, center.z,
                              endPos.x, endPos.y, endPos.z});
    };

    addHandle(GizmoAxis::X, glm::vec3(1,0,0), extents.x);
    addHandle(GizmoAxis::X, glm::vec3(-1,0,0), extents.x);
    addHandle(GizmoAxis::Y, glm::vec3(0,1,0), extents.y);
    addHandle(GizmoAxis::Y, glm::vec3(0,-1,0), extents.y);
    addHandle(GizmoAxis::Z, glm::vec3(0,0,1), extents.z);
    addHandle(GizmoAxis::Z, glm::vec3(0,0,-1), extents.z);

    auto colorFor = [&](const glm::vec3& base, GizmoAxis axis, int sign)
    {
        if (boundingBoxActiveAxis == axis && boundingBoxDragSign == sign)
            return base * 1.3f;
        return base;
    };

    for (size_t i = 0; i < gizmoLines.size(); ++i)
    {
        GizmoAxis axis = GizmoAxis::X;
        int sign = 1;
        if (i == 0 || i == 1) { axis = GizmoAxis::X; sign = i == 0 ? 1 : -1; }
        else if (i == 2 || i == 3) { axis = GizmoAxis::Y; sign = i == 2 ? 1 : -1; }
        else { axis = GizmoAxis::Z; sign = i == 4 ? 1 : -1; }

        glm::vec3 base = axis == GizmoAxis::X ? glm::vec3(1,0,0) :
                         axis == GizmoAxis::Y ? glm::vec3(0,1,0) :
                         glm::vec3(0,0,1);

        glm::vec3 col = colorFor(base, axis, sign);
        renderer->drawNavmeshLines({gizmoLines[i]}, col, 4.0f);
    }
}


bool ViewerApp::Init()
{
    if (!InitSDL()) return false;
    if (!InitGL())  return false;
    if (!InitImGui()) return false;

    camera   = new ViewerCamera();
    renderer = new RendererGL();

    LoadLastDirectory();

    return true;
}


void ViewerApp::Shutdown()
{
    JoinNavmeshWorker();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    delete camera;
    delete renderer;
}

bool ViewerApp::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return false;
    }

    // OpenGL 3.3 Core
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);    

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);


    window = SDL_CreateWindow(
        "GTA NavMesh Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1600, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window)
    {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // vsync
    printf("window = %p\n", window);
    printf("glContext = %p\n", glContext);

    return true;
}

bool ViewerApp::InitGL()
{
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_Log("Failed to initialize GLAD");
        return false;
    }

    glViewport(0, 0, 1600, 900);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LESS); // já deve ser default, mas garantimos

    return true;
}

bool ViewerApp::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // POR ENQUANTO: sem docking nem viewports
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

bool ViewerApp::LoadMeshFromPath(const std::string& path)
{
    return LoadMeshFromPathWithOptions(path, centerMesh, preferBin);
}

bool ViewerApp::LoadMeshFromPathWithOptions(const std::string& path, bool center, bool tryLoadBin)
{
    if (IsNavmeshJobRunning())
    {
        printf("[ViewerApp] Carregamento de mesh bloqueado: navmesh está em construção.\n");
        return false;
    }

    std::unique_ptr<Mesh> newMesh(ObjLoader::LoadMesh(path, center, tryLoadBin));
    if (!newMesh)
    {
        printf("Falha ao carregar OBJ!\n");
        return false;
    }

    MeshInstance instance;
    instance.name = std::filesystem::path(path).stem().string();
    instance.sourcePath = path;
    instance.id = CurrentNextMeshId()++;
    instance.mesh = std::move(newMesh);
    CurrentMeshes().push_back(std::move(instance));
    CurrentPickedTri() = -1;
    CurrentPickedMeshIndex() = -1;

    printf("OBJ carregado: %zu verts, %zu indices\n",
           CurrentMeshes().back().mesh->renderVertices.size(),
           CurrentMeshes().back().mesh->indices.size());

    HandleAutoBuild(NavmeshAutoBuildFlag::OnAdd);

    std::filesystem::path p(path);
    SaveLastDirectory(p.parent_path());

    return true;
}

std::filesystem::path ViewerApp::GetConfigFilePath() const
{
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::filesystem::path result = std::filesystem::path(basePath) / "last_directory.txt";
        SDL_free(basePath);
        return result;
    }

    return "last_directory.txt";
}

std::filesystem::path ViewerApp::ResolveMemoryHandlerObjPath(const std::string& propName) const
{
    if (memoryHandler.GetObjDirectory().empty())
        return {};

    std::filesystem::path base = memoryHandler.GetObjDirectory();
    std::filesystem::path direct = base / propName;
    if (std::filesystem::exists(direct))
        return direct;

    std::filesystem::path withExt = direct;
    if (!withExt.has_extension())
        withExt.replace_extension(".obj");

    if (std::filesystem::exists(withExt))
        return withExt;

    return {};
}

void ViewerApp::LoadLastDirectory()
{
    std::filesystem::path configPath = GetConfigFilePath();
    std::ifstream file(configPath);
    if (file)
    {
        std::string storedPath;
        std::getline(file, storedPath);
        if (!storedPath.empty() && std::filesystem::exists(storedPath))
        {
            currentDirectory = storedPath;
        }
    }
}

void ViewerApp::SaveLastDirectory(const std::filesystem::path& directory)
{
    if (directory.empty()) return;

    std::filesystem::path configPath = GetConfigFilePath();
    std::ofstream file(configPath, std::ios::trunc);
    if (file)
    {
        file << directory.string();
    }
}

void ViewerApp::HandleAutoBuild(NavmeshAutoBuildFlag flag)
{
    if (flag == NavmeshAutoBuildFlag::None)
        return;

    if (static_cast<bool>(navmeshAutoBuildMask & static_cast<uint32_t>(flag)))
    {
        buildNavmeshFromMeshes();
    }
}

bool ViewerApp::IsMeshAlreadyLoaded(const std::filesystem::path& path) const
{
    const auto& meshes = CurrentMeshes();
    for (const auto& instance : meshes)
    {
        if (instance.sourcePath == path)
            return true;
    }
    return false;
}


void ViewerApp::RemoveMesh(size_t index)
{
    auto& meshes = CurrentMeshes();
    if (index >= meshes.size())
        return;

    uint64_t removedId = meshes[index].id;
    DetachChildren(removedId);
    CurrentMeshStateCache().erase(removedId);
    meshes.erase(meshes.begin() + index);
    if (CurrentPickedMeshIndex() == static_cast<int>(index))
    {
        CurrentPickedMeshIndex() = -1;
        CurrentPickedTri() = -1;
    }
    else if (CurrentPickedMeshIndex() > static_cast<int>(index))
    {
        CurrentPickedMeshIndex()--;
    }

    HandleAutoBuild(NavmeshAutoBuildFlag::OnRemove);

    if (meshes.empty())
    {
        ClearNavmeshSlotData(currentGeometrySlot);
    }
}

void ViewerApp::ClearMeshes()
{
    auto& meshes = CurrentMeshes();
    if (meshes.empty())
        return;

    ClearNavmeshSlotData(currentGeometrySlot);
    HandleAutoBuild(NavmeshAutoBuildFlag::OnRemove);
}


void ViewerApp::Run()
{
    while (running)
    {
        ProcessEvents();
        RenderFrame();
    }
}

void ViewerApp::JoinNavmeshWorker()
{
    if (navmeshWorker.joinable())
    {
        navmeshWorker.join();
    }
}

void ViewerApp::RequestStopNavmeshBuild()
{
    if (!IsNavmeshJobRunning())
        return;

    navmeshCancelRequested.store(true);
    navmeshJobQueued = false;
    printf("[ViewerApp] Cancelamento do build de navmesh solicitado.\n");
}

void ViewerApp::ClearNavmesh()
{
    if (IsNavmeshJobRunning())
        return;

    JoinNavmeshWorker();

    if (CurrentNavQuery())
    {
        dtFreeNavMeshQuery(CurrentNavQuery());
        CurrentNavQuery() = nullptr;
    }

    CurrentNavData() = {};
    CurrentNavMeshTris().clear();
    CurrentNavMeshLines().clear();
    CurrentNavmeshLineBuffer().clear();
    CurrentOffmeshLinkLines().clear();
    CurrentNavQueryReady() = false;
    ResetPathState(currentNavmeshSlot);

    printf("[ViewerApp] Navmesh do slot %d limpo.\n", currentNavmeshSlot);
}

void ViewerApp::ProcessNavmeshJob()
{
    if (!navmeshJobCompleted.load())
        return;

    JoinNavmeshWorker();
    navmeshJobCompleted.store(false);

    std::unique_ptr<NavmeshJobResult> result;
    {
        std::lock_guard<std::mutex> lock(navmeshJobMutex);
        result = std::move(navmeshJobResult);
    }

    if (result && result->success)
    {
        if (!IsValidSlot(result->slotIndex))
        {
            printf("[ViewerApp] Navmesh worker retornou slot inválido %d.\n", result->slotIndex);
        }
        else
        {
            const int previousSlot = currentNavmeshSlot;
            currentNavmeshSlot = result->slotIndex;
            CurrentNavData() = std::move(result->navData);
            CurrentNavMeshTris() = std::move(result->tris);
            CurrentNavMeshLines() = std::move(result->lines);
            RebuildDebugLineBuffer(result->slotIndex);
            CurrentNavQueryReady() = false;

            const bool queryOk = InitNavQueryForCurrentNavmesh();
            if (queryOk && CurrentHasPathStart() && CurrentHasPathTarget())
            {
                TryRunPathfind();
            }
            else if (!queryOk)
            {
                ResetPathState();
            }

            currentNavmeshSlot = previousSlot;

            printf("[ViewerApp] Navmesh worker aplicado na thread principal.\n");
        }
    }
    else if (result)
    {
        printf("[ViewerApp] Navmesh worker falhou. Mantendo navmesh atual.\n");
    }

    if (navmeshJobQueued)
    {
        bool buildTilesNow = navmeshJobQueuedBuildTilesNow;
        int queuedSlot = navmeshJobQueuedSlot >= 0 ? navmeshJobQueuedSlot : currentNavmeshSlot;
        navmeshJobQueued = false;
        navmeshJobQueuedSlot = -1;
        buildNavmeshFromMeshes(buildTilesNow, queuedSlot);
    }
}

void ViewerApp::ProcessMemoryGeometryRequests()
{
    if (!memoryHandler.IsMonitoringActive() || IsNavmeshJobRunning())
        return;

    std::vector<MemoryHandler::GeometrySlot> slots;
    if (!memoryHandler.FetchGeometrySlots(slots))
        return;

    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
    {
        auto& slot = slots[static_cast<size_t>(i)];

        if (slot.remove)
        {
            auto mapIt = memorySlotToMeshId.find(i);
            if (mapIt != memorySlotToMeshId.end())
            {
                int meshIdx = FindMeshIndexById(mapIt->second);
                if (meshIdx >= 0)
                {
                    RemoveMesh(static_cast<size_t>(meshIdx));
                }
                memorySlotToMeshId.erase(mapIt);
            }
            memoryHandler.ClearGeometrySlot(i);
            continue;
        }

        if (!slot.update)
            continue;

        std::string hashStr = std::to_string(static_cast<int>(slot.modelHash));
        std::string propName;
        if (!memoryHandler.TryResolvePropName(hashStr, propName))
        {
            slot.update = 0;
            memoryHandler.WriteGeometrySlot(i, slot);
            continue;
        }

        std::filesystem::path objPath = ResolveMemoryHandlerObjPath(propName);
        if (objPath.empty())
        {
            slot.update = 0;
            memoryHandler.WriteGeometrySlot(i, slot);
            continue;
        }

        uint64_t meshId = 0;
        MeshInstance* targetInstance = nullptr;

        auto existingMap = memorySlotToMeshId.find(i);
        if (existingMap != memorySlotToMeshId.end())
        {
            meshId = existingMap->second;
            targetInstance = FindMeshById(meshId);
        }

        if (!targetInstance)
        {

            if (!LoadMeshFromPathWithOptions(objPath.string(), centerMesh, preferBin))
                continue;

            targetInstance = &CurrentMeshes().back();
            memorySlotToMeshId[i] = targetInstance->id;
            meshId = targetInstance->id;
        }

        int targetIndex = FindMeshIndexById(meshId);
        if (targetIndex < 0 || !targetInstance)
            continue;

        glm::vec3 newPos = glm::vec3(slot.position.x, slot.position.y, slot.position.z);
        glm::vec3 newRot = glm::vec3(slot.rotation.x, slot.rotation.y, slot.rotation.z);

        bool posChanged = targetInstance->position != newPos;
        bool rotChanged = targetInstance->rotation != newRot;

        targetInstance->position = newPos;
        targetInstance->rotation = newRot;

        if (slot.parentId != 0)
        {
            auto parentIt = memorySlotToMeshId.find(static_cast<int>(slot.parentId));
            if (parentIt != memorySlotToMeshId.end())
            {
                SetMeshParent(static_cast<size_t>(targetIndex), parentIt->second);
            }
        }

        OnInstanceTransformUpdated(static_cast<size_t>(targetIndex), posChanged, rotChanged);

        slot.update = 0;
        memoryHandler.WriteGeometrySlot(i, slot);
    }
}

void ViewerApp::ProcessMemoryRouteRequests()
{
    if (!memoryHandler.IsMonitoringActive() || IsNavmeshJobRunning())
        return;

    std::vector<MemoryHandler::RouteRequestSlot> requests;
    if (!memoryHandler.FetchRouteRequests(requests))
        return;

    const bool hasNavmesh = CurrentNavData().IsLoaded();
    if (hasNavmesh && !CurrentNavQueryReady())
    {
        InitNavQueryForCurrentNavmesh();
    }

    for (int i = 0; i < static_cast<int>(requests.size()); ++i)
    {
        const auto& slot = requests[static_cast<size_t>(i)];
        if (!slot.request)
            continue;

        MemoryHandler::RouteRequestSlot writable = slot;
        glm::vec3 startInternal = glm::vec3(slot.start.x, slot.start.y, slot.start.z);
        glm::vec3 targetInternal = glm::vec3(slot.target.x, slot.target.y, slot.target.z);
        const float extents[3]  = { navGenSettings.agentRadius * 4.0f + 0.1f, navGenSettings.agentHeight * 0.5f + 0.1f, navGenSettings.agentRadius * 4.0f + 0.1f };

        // Immediately reset request and payload to keep protocol functional.
        writable.request = 0;
        writable.start = {};
        writable.target = {};
        writable.flags = 0;
        writable.state = 1; // busy
        memoryHandler.WriteRouteRequestSlot(i, writable);

        auto finishWithEmpty = [&](int newState)
        {
            memoryHandler.WriteRouteResultPoints(i, {});
            writable.state = newState;
            memoryHandler.WriteRouteRequestSlot(i, writable);
        };

        if (!hasNavmesh || !CurrentNavQuery())
        {
            finishWithEmpty(2);
            continue;
        }

        dtPolyRef startRef = 0, endRef = 0;
        float startNearest[3]{};
        float endNearest[3]{};

        const float startPos[3] = { startInternal.x, startInternal.y, startInternal.z };
        const float endPos[3]   = { targetInternal.x, targetInternal.y, targetInternal.z };

        if (dtStatusFailed(CurrentNavQuery()->findNearestPoly(startPos, extents, &pathQueryFilter, &startRef, startNearest)) || startRef == 0)
        {
            finishWithEmpty(2);
            continue;
        }

        if (dtStatusFailed(CurrentNavQuery()->findNearestPoly(endPos, extents, &pathQueryFilter, &endRef, endNearest)) || endRef == 0)
        {
            finishWithEmpty(2);
            continue;
        }

        dtPolyRef polys[256];
        int polyCount = 0;
        const dtStatus pathStatus = CurrentNavQuery()->findPath(startRef, endRef, startNearest, endNearest, &pathQueryFilter, polys, &polyCount, 256);
        if (dtStatusFailed(pathStatus) || polyCount == 0)
        {
            finishWithEmpty(2);
            continue;
        }

        float straight[256 * 3];
        unsigned char straightFlags[256];
        dtPolyRef straightRefs[256];
        int straightCount = 0;

        dtStatus straightStatus = DT_FAILURE;
        if (slot.minEdgeDistance > 0.0f)
        {
            straightStatus = CurrentNavQuery()->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, straight, straightFlags, straightRefs, &straightCount, 256, slot.flags, slot.minEdgeDistance);
        }
        else
        {
            straightStatus = CurrentNavQuery()->findStraightPath(startNearest, endNearest, polys, polyCount, straight, straightFlags, straightRefs, &straightCount, 256, slot.flags);
        }

        if (dtStatusFailed(straightStatus) || straightCount == 0)
        {
            finishWithEmpty(2);
            continue;
        }

        std::vector<MemoryHandler::Vector3> gtaPoints;
        gtaPoints.reserve(static_cast<size_t>(straightCount));
        for (int p = 0; p < straightCount; ++p)
        {
            glm::vec3 internalPoint(straight[p * 3 + 0], straight[p * 3 + 1], straight[p * 3 + 2]);
            glm::vec3 gtaPoint = ToGtaCoords(internalPoint);
            MemoryHandler::Vector3 v{};
            v.x = gtaPoint.x;
            v.y = gtaPoint.y;
            v.z = gtaPoint.z;
            gtaPoints.push_back(v);
        }

        memoryHandler.WriteRouteResultPoints(i, gtaPoints);

        writable.state = 2; // finished
        writable.pointsCount = straightCount; 
        memoryHandler.WriteRouteRequestSlot(i, writable);
    }
}





void ViewerApp::RenderFrame()
{
    ProcessNavmeshJob();

    RunProceduralTestStep();

    memoryHandler.Tick();
    ProcessMemoryGeometryRequests();
    ProcessMemoryRouteRequests();

    // --- Iniciar nova frame ImGui ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (!IsNavmeshJobRunning())
        UpdateNavmeshTiles();

    const bool navmeshBusy = IsNavmeshJobRunning();
    const bool navmeshStopRequested = navmeshCancelRequested.load();

    if (viewportClickMode == ViewportClickMode::EditMesh && editDragActive && leftMouseDown)
    {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        UpdateEditDrag(mx, my);
    }
    else if (viewportClickMode == ViewportClickMode::EditBoundingBoxSize && boundingBoxDragActive && leftMouseDown)
    {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        UpdateBoundingBoxDrag(mx, my);
    }
    else if (!leftMouseDown && editDragActive)
    {
        editDragActive = false;
        activeGizmoAxis = GizmoAxis::None;
    }
    else if (!leftMouseDown && boundingBoxDragActive)
    {
        boundingBoxDragActive = false;
        boundingBoxActiveAxis = GizmoAxis::None;
    }

    // --- Barra de menus principal ---
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Arquivo"))
        {
            ImGui::MenuItem("Browser de Meshes", nullptr, &showMeshBrowserWindow);
            ImGui::MenuItem("Painel Lateral", nullptr, &showSidePanel);
            ImGui::Separator();
            ImGui::MenuItem("Centralizar mesh carregada", nullptr, &centerMesh);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Visualização"))
        {
            ImGui::TextDisabled("Render Mode");
            int mode = static_cast<int>(renderMode);
            ImGui::RadioButton("Solid (F1)", &mode, 0);
            ImGui::RadioButton("Wireframe (F2)", &mode, 1);
            ImGui::RadioButton("Normals (F3)", &mode, 2);
            ImGui::RadioButton("Depth (F4)", &mode, 3);
            ImGui::RadioButton("Lit (F5)", &mode, 4);
            renderMode = static_cast<RenderMode>(mode);

            ImGui::Separator();
            ImGui::MenuItem("Mostrar navmesh", nullptr, &navmeshVisible);
            ImGui::MenuItem("Mostrar debug/câmera", nullptr, &showDebugInfo);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Navmesh"))
        {
            bool hasMesh = !CurrentMeshes().empty();
            bool hasNavmesh = CurrentNavData().IsLoaded();
            ImGui::BeginDisabled(!hasMesh || navmeshBusy);
            if (ImGui::MenuItem("Build Navmesh"))
            {
                buildNavmeshFromMeshes(true, currentNavmeshSlot);
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(navmeshBusy || !hasNavmesh);
            if (ImGui::MenuItem("Clear Navmesh"))
            {
                ClearNavmesh();
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(!navmeshBusy);
            if (ImGui::MenuItem("Stop Build"))
            {
                RequestStopNavmeshBuild();
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::BeginDisabled(navmeshBusy);
            bool autoOnAdd    = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd)) != 0;
            bool autoOnRemove = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove)) != 0;
            bool autoOnMove   = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove)) != 0;
            bool autoOnRotate = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate)) != 0;

            if (ImGui::MenuItem("Rebuild ao adicionar", nullptr, &autoOnAdd))
            {
                if (autoOnAdd)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
            }
            if (ImGui::MenuItem("Rebuild ao remover", nullptr, &autoOnRemove))
            {
                if (autoOnRemove)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
            }
            if (ImGui::MenuItem("Rebuild ao mover", nullptr, &autoOnMove))
            {
                if (autoOnMove)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
            }
            if (ImGui::MenuItem("Rebuild ao rotacionar", nullptr, &autoOnRotate))
            {
                if (autoOnRotate)
                    navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                else
                    navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
            }
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Slot"))
        {
            for (int slot = 0; slot < kMaxNavmeshSlots; ++slot)
            {
                char label[8];
                snprintf(label, sizeof(label), "%d", slot);
                const bool selected = currentNavmeshSlot == slot;
                if (ImGui::MenuItem(label, nullptr, selected))
                {
                    SwitchNavmeshSlot(slot);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GTA Tracker"))
        {
            bool monitorGta = memoryHandler.IsMonitoringRequested();
            if (ImGui::MenuItem("Monitorar GTA", nullptr, &monitorGta))
            {
                memoryHandler.SetMonitoringEnabled(monitorGta);
            }

            ImGui::Separator();
            ImGui::TextWrapped("%s", memoryHandler.GetStatus().c_str());
            const auto layoutPath = memoryHandler.GetLayoutFilePath();
            if (!layoutPath.empty())
            {
                ImGui::TextDisabled("Layout JSON: %s", layoutPath.string().c_str());
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ajuda"))
        {
            ImGui::MenuItem("Debug/Diagnóstico", nullptr, &showDebugInfo);
            ImGui::TextDisabled("Use TABs no painel lateral para alternar modos.");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    const bool hasNavmeshLoaded = CurrentNavData().IsLoaded();
    auto& navmeshTris = CurrentNavMeshTris();
    auto& navmeshLineBuffer = CurrentNavmeshLineBuffer();
    auto& offmeshLines = CurrentOffmeshLinkLines();
    auto& pathLines = CurrentPathLines();

    // --- OBJ Browser ---
    if (showMeshBrowserWindow)
    {
        ImGui::Begin("Mesh Browser", &showMeshBrowserWindow);
        if (!std::filesystem::exists(currentDirectory))
        {
            currentDirectory = std::filesystem::current_path().string();
        }

        if (navmeshBusy)
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Carregamento bloqueado: navmesh em construção.");
            ImGui::BeginDisabled();
        }

        ImGui::Text("Diretório: %s", currentDirectory.c_str());
        ImGui::Checkbox("Centralizar mesh carregada", &centerMesh);

        if (ImGui::Button("Selecionar pasta"))
        {
            directoryBrowser.SetDirectory(currentDirectory);
            directoryBrowser.Open();
        }

        if (ImGui::Button("Subir"))
        {
            std::filesystem::path parent = std::filesystem::path(currentDirectory).parent_path();
            if (!parent.empty())
            {
                currentDirectory = parent.string();
                selectedEntry.clear();
            }
        }

        std::error_code ec;
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> objFiles;
        for (const auto& entry : std::filesystem::directory_iterator(currentDirectory, ec))
        {
            if (entry.is_directory())
            {
                directories.push_back(entry);
            }
            else
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (ext == ".obj")
                    objFiles.push_back(entry);
            }
        }

        std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b){ return a.path().filename() < b.path().filename(); });
        std::sort(objFiles.begin(), objFiles.end(), [](const auto& a, const auto& b){ return a.path().filename() < b.path().filename(); });

        ImGui::SeparatorText("Pastas");
        for (const auto& dir : directories)
        {
            std::string label = "[DIR] " + dir.path().filename().string();
            bool selected = selectedEntry == dir.path().string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedEntry = dir.path().string();
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    currentDirectory = dir.path().string();
                    selectedEntry.clear();
                }
            }
        }

        ImGui::SeparatorText("Arquivos OBJ");
        for (const auto& file : objFiles)
        {
            std::string label = file.path().filename().string();
            bool selected = selectedEntry == file.path().string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedEntry = file.path().string();
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    LoadMeshFromPath(selectedEntry);
                }
            }
        }

        if (!ec)
        {
            if (ImGui::Button("Carregar OBJ selecionado"))
            {
                std::filesystem::path p(selectedEntry);
                auto ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (ext == ".obj")
                {
                    LoadMeshFromPath(selectedEntry);
                }
            }

            ImGui::Checkbox("Preferir BIN", &preferBin);
        }
        else
        {
            ImGui::TextColored(ImVec4(1,0,0,1), "Erro ao listar diretório: %s", ec.message().c_str());
        }

        directoryBrowser.Display();
        if (directoryBrowser.HasSelected())
        {
            currentDirectory = directoryBrowser.GetSelected().string();
            selectedEntry.clear();
            directoryBrowser.ClearSelected();
        }

        if (navmeshBusy)
            ImGui::EndDisabled();

        ImGui::End();
    }

    // --- Painel lateral com tabs ---
    if (showSidePanel)
    {
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Painel", &showSidePanel, ImGuiWindowFlags_NoCollapse);

        if (ImGui::BeginTabBar("PainelTabs"))
        {
            if (ImGui::BeginTabItem("Edição"))
            {
                if (ImGui::CollapsingHeader("Renderização", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int mode = static_cast<int>(renderMode);
                    ImGui::RadioButton("Solid (F1)", &mode, 0);
                    ImGui::RadioButton("Wireframe (F2)", &mode, 1);
                    ImGui::RadioButton("Normals (F3)", &mode, 2);
                    ImGui::RadioButton("Depth (F4)", &mode, 3);
                    ImGui::RadioButton("Lit (F5)", &mode, 4);
                    renderMode = static_cast<RenderMode>(mode);
                }

                if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("Comportamento do clique");
                    ViewportClickMode previousMode = viewportClickMode;
                    int viewportModeValue = static_cast<int>(viewportClickMode);

                    ImGui::RadioButton("pickTriangleMode", &viewportModeValue, static_cast<int>(ViewportClickMode::PickTriangle));
                    ImGui::RadioButton("buildTileAtMode", &viewportModeValue, static_cast<int>(ViewportClickMode::BuildTileAt));
                    ImGui::RadioButton("removeTileMode", &viewportModeValue, static_cast<int>(ViewportClickMode::RemoveTileAt));
                    ImGui::RadioButton("Pathfind", &viewportModeValue, static_cast<int>(ViewportClickMode::Pathfind_Normal));
                    ImGui::RadioButton("Pathfind With Min Edge Distance", &viewportModeValue, static_cast<int>(ViewportClickMode::Pathfind_MinEdge));
                    ImGui::RadioButton("Edit Mesh", &viewportModeValue, static_cast<int>(ViewportClickMode::EditMesh));
                    ImGui::RadioButton("Add Offmesh Link", &viewportModeValue, static_cast<int>(ViewportClickMode::AddOffmeshLink));
                    ImGui::RadioButton("Remove Offmesh Link", &viewportModeValue, static_cast<int>(ViewportClickMode::RemoveOffmeshLink));
                    ImGui::RadioButton("Set Bounding Box", &viewportModeValue, static_cast<int>(ViewportClickMode::SetBoundingBox));
                    ImGui::RadioButton("Edit Bounding Box Size", &viewportModeValue, static_cast<int>(ViewportClickMode::EditBoundingBoxSize));

                    viewportClickMode = static_cast<ViewportClickMode>(viewportModeValue);
                    ImGui::BeginDisabled(viewportClickMode != ViewportClickMode::Pathfind_MinEdge);
                    ImGui::SliderFloat("Min Edge Distance", &pathfindMinEdgeDistance, 0.0f, 100.0f, "%.2f");
                    ImGui::EndDisabled();

                    if (viewportClickMode != ViewportClickMode::EditMesh)
                    {
                        editDragActive = false;
                        activeGizmoAxis = GizmoAxis::None;
                        boundingBoxDragActive = false;
                        boundingBoxActiveAxis = GizmoAxis::None;
                        if (previousMode == ViewportClickMode::EditMesh)
                        {
                            CurrentPickedMeshIndex() = -1;
                            CurrentPickedTri() = -1;
                        }
                    }

                    if (previousMode == ViewportClickMode::AddOffmeshLink && viewportClickMode != ViewportClickMode::AddOffmeshLink)
                    {
                        CurrentHasOffmeshStart() = false;
                        CurrentHasOffmeshTarget() = false;
                    }

                    if (viewportClickMode == ViewportClickMode::EditMesh)
                    {
                        int editModeValue = static_cast<int>(meshEditMode);
                        ImGui::SeparatorText("Mesh Edit Mode");
                        ImGui::RadioButton("Move", &editModeValue, static_cast<int>(MeshEditMode::Move));
                        ImGui::RadioButton("Rotate", &editModeValue, static_cast<int>(MeshEditMode::Rotate));
                        meshEditMode = static_cast<MeshEditMode>(editModeValue);

                        ImGui::Checkbox("Draw Mesh Outlines", &drawMeshOutlines);

                        if (meshEditMode == MeshEditMode::Move)
                        {
                            int moveMode = static_cast<int>(moveTransformMode);
                            ImGui::RadioButton("Global", &moveMode, static_cast<int>(MoveTransformMode::GlobalTransform));
                            ImGui::RadioButton("Local", &moveMode, static_cast<int>(MoveTransformMode::LocalTransform));
                            moveTransformMode = static_cast<MoveTransformMode>(moveMode);
                        }
                    }

                    if (!IsPathfindModeActive() && (previousMode == ViewportClickMode::Pathfind_Normal || previousMode == ViewportClickMode::Pathfind_MinEdge))
                    {
                        ResetPathState();
                    }

                    const char* activeMode = "";
                    switch (viewportClickMode)
                    {
                    case ViewportClickMode::PickTriangle: activeMode = "pickTriangleMode"; break;
                    case ViewportClickMode::BuildTileAt: activeMode = "buildTileAtMode"; break;
                    case ViewportClickMode::RemoveTileAt: activeMode = "removeTileMode"; break;
                    case ViewportClickMode::Pathfind_Normal: activeMode = "Pathfind"; break;
                    case ViewportClickMode::Pathfind_MinEdge: activeMode = "Pathfind With Min Edge Distance"; break;
                    case ViewportClickMode::EditMesh: activeMode = "Edit Mesh"; break;
                    case ViewportClickMode::AddOffmeshLink: activeMode = "Add Offmesh Link"; break;
                    case ViewportClickMode::RemoveOffmeshLink: activeMode = "Remove Offmesh Link"; break;
                    case ViewportClickMode::SetBoundingBox: activeMode = "Set Bounding Box"; break;
                    case ViewportClickMode::EditBoundingBoxSize: activeMode = "Edit Bounding Box Size"; break;
                    }

                    ImGui::Text("Modo ativo: %s", activeMode);
                    ImGui::Text("Start: %s | Target: %s", CurrentHasPathStart() ? "definido" : "pendente", CurrentHasPathTarget() ? "definido" : "pendente");
                    ImGui::BeginDisabled(viewportClickMode != ViewportClickMode::AddOffmeshLink);
                    ImGui::Checkbox("BiDir", &offmeshBidirectional);
                    ImGui::Text("Offmesh Start: %s | Offmesh Target: %s", CurrentHasOffmeshStart() ? "definido" : "pendente", CurrentHasOffmeshTarget() ? "definido" : "pendente");
                    ImGui::EndDisabled();

                    ImGui::BeginDisabled(CurrentNavData().GetOffmeshLinks().empty());
                    if (ImGui::Button("Limpar Offmesh Links"))
                    {
                        CurrentNavData().ClearOffmeshLinks();
                        RebuildOffmeshLinkLines();
                        CurrentHasOffmeshStart() = false;
                        CurrentHasOffmeshTarget() = false;
                        printf("[ViewerApp] Offmesh links limpos.\n");

                        if (CurrentNavData().IsLoaded())
                        {
                            buildNavmeshFromMeshes(true, currentNavmeshSlot);
                        }
                    }
                    ImGui::EndDisabled();

                    if (ImGui::BeginTable("BBoxControls", 1, ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::SeparatorText("Bounding Box");
                        ImGui::TextWrapped("Use \"Set Bounding Box\" para dois cliques no viewport.");
                        if (ImGui::Button("Remover Bounding Box"))
                        {
                            ClearBoundingBox();
                        }
                        ImGui::SliderFloat("Bounding Box Alpha", &boundingBoxAlpha, 0.0f, 1.0f, "%.2f");
                        ImGui::BeginDisabled(!boundingBoxVisible);
                        float p0[3] = { boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z };
                        float p1[3] = { boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z };
                        if (ImGui::InputFloat3("Bounding Box Min", p0) |
                            ImGui::InputFloat3("Bounding Box Max", p1))
                        {
                            boundingBoxMin = glm::vec3(p0[0], p0[1], p0[2]);
                            boundingBoxMax = glm::vec3(p1[0], p1[1], p1[2]);
                            boundingBoxP0 = boundingBoxMin;
                            boundingBoxP1 = boundingBoxMax;
                            RebuildBoundingBoxDebug();
                        }
                        ImGui::EndDisabled();
                        ImGui::EndTable();
                    }
                }

                if (ImGui::CollapsingHeader("GTA Handler", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    gtaHandlerMenu.Draw(gtaHandler, *this);
                }

                if (ImGui::CollapsingHeader("Memory Handler", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool monitorGta = memoryHandler.IsMonitoringRequested();
                    if (ImGui::Checkbox("Monitorar GTA", &monitorGta))
                    {
                        memoryHandler.SetMonitoringEnabled(monitorGta);
                    }

                    ImGui::TextWrapped("%s", memoryHandler.GetStatus().c_str());
                    const auto layoutPath = memoryHandler.GetLayoutFilePath();
                    if (!layoutPath.empty())
                    {
                        ImGui::TextDisabled("Layout JSON: %s", layoutPath.string().c_str());
                    }

                    if (ImGui::Button("Selecionar PropHashToName.json"))
                    {
                        propHashBrowser.SetPwd(memoryHandler.GetPropHashFile().empty() ?
                            std::filesystem::current_path() : memoryHandler.GetPropHashFile().parent_path());
                        propHashBrowser.Open();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(memoryHandler.GetPropHashFile().empty() ? "(nenhum arquivo selecionado)" : memoryHandler.GetPropHashFile().string().c_str());

                    if (ImGui::Button("Selecionar pasta OBJ (Memory Handler)"))
                    {
                        objDirectoryBrowser.SetPwd(memoryHandler.GetObjDirectory().empty() ?
                            std::filesystem::current_path() : memoryHandler.GetObjDirectory());
                        objDirectoryBrowser.Open();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(memoryHandler.GetObjDirectory().empty() ? "(nenhuma pasta selecionada)" : memoryHandler.GetObjDirectory().string().c_str());

                    ImGui::BeginDisabled(memoryHandler.GetPropHashFile().empty());
                    if (ImGui::Button("Carregar PropHashToName"))
                    {
                        memoryHandler.LoadPropHashMapping();
                    }
                    ImGui::EndDisabled();

                    ImGui::TextDisabled("Usa arquivo de hash para nome + pasta OBJ para carregar meshes automaticamente.");
                }

                if (ImGui::CollapsingHeader("Navmesh Tests"))
                {
                    ImGui::BeginDisabled(navmeshBusy);
                    bool updateTiles = navmeshUpdateTiles;
                    if (ImGui::Checkbox("Update Tiles", &updateTiles))
                    {
                        navmeshUpdateTiles = updateTiles;
                        CurrentMeshStateCache().clear();
                        if (navmeshUpdateTiles)
                        {
                            for (const auto& instance : CurrentMeshes())
                            {
                                MeshBoundsState state = ComputeMeshBounds(instance);
                                if (state.valid)
                                {
                                    CurrentMeshStateCache()[instance.id] = state;
                                }
                            }
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(reconstroi tiles com geometria alterada)");

                    if (ImGui::Button("experimental_buildSingleTile"))
                    {
                        if (navGenSettings.mode != NavmeshBuildMode::Tiled)
                        {
                            printf("[ViewerApp] experimental_buildSingleTile: forçando modo tiled.\n");
                            navGenSettings.mode = NavmeshBuildMode::Tiled;
                        }

                        if (buildNavmeshFromMeshes(false))
                        {
                            if (IsPathfindModeActive())
                                ResetPathState();
                            viewportClickMode = ViewportClickMode::BuildTileAt;
                            printf("[ViewerApp] Navmesh tiled inicializado sem tiles. Clique na mesh para gerar tiles individuais.\n");
                        }
                        else
                        {
                            printf("[ViewerApp] experimental_buildSingleTile falhou ao preparar navmesh tiled.\n");
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(prepara grid/caches e gera tiles no clique)");
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::BeginCombo("Geometria do Slot", std::to_string(currentGeometrySlot).c_str()))
                    {
                        for (int slot = 0; slot < kMaxNavmeshSlots; ++slot)
                        {
                            bool selected = currentGeometrySlot == slot;
                            char label[16];
                            snprintf(label, sizeof(label), "%d", slot);
                            if (ImGui::Selectable(label, selected))
                            {
                                currentGeometrySlot = slot;
                                geometrySlotForNavSlot[currentNavmeshSlot] = slot;
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::BeginDisabled(navmeshBusy);
                    ImGui::InputTextWithHint("##meshFilter", "Filtrar por nome", meshFilter, IM_ARRAYSIZE(meshFilter));
                    ImGui::BeginDisabled(CurrentMeshes().empty());
                    if (ImGui::Button("Limpar Meshes"))
                    {
                        ClearMeshes();
                    }
                    ImGui::EndDisabled();
                    if (ImGui::BeginTable("MeshTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableSetupColumn("Mesh");
                        ImGui::TableSetupColumn("Transform");
                        ImGui::TableSetupColumn("Ações", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i = 0; i < CurrentMeshes().size(); )
                        {
                            auto& instance = CurrentMeshes()[i];
                            std::string lowerName = instance.name;
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                            std::string lowerFilter = meshFilter;
                            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), [](unsigned char c){ return (char)std::tolower(c); });

                            if (!lowerFilter.empty() && lowerName.find(lowerFilter) == std::string::npos)
                            {
                                ++i;
                                continue;
                            }

                            bool allowMeshSelection = viewportClickMode == ViewportClickMode::EditMesh;

                            ImGui::PushID(static_cast<int>(i));
                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            bool isSelected = allowMeshSelection && static_cast<int>(i) == CurrentPickedMeshIndex();
                            ImGui::BeginDisabled(!allowMeshSelection);
                            if (ImGui::Selectable(instance.name.c_str(), isSelected))
                            {
                                CurrentPickedMeshIndex() = static_cast<int>(i);
                                CurrentPickedTri() = -1;
                            }
                            ImGui::EndDisabled();

                            ImGui::TableNextColumn();
                            glm::vec3 previousPos = instance.position;
                            glm::vec3 previousRot = instance.rotation;
                            glm::vec3 gtaPos = ToGtaCoords(instance.position);
                            glm::vec3 previousGtaPos = ToGtaCoords(previousPos);
                            bool moved = ImGui::DragFloat3("Pos", &gtaPos.x, 0.25f);
                            bool rotated = ImGui::DragFloat3("Rot", &instance.rotation.x, 0.25f);
                            if (rotated)
                            {
                                NormalizeMeshRotation(instance);
                            }

                            if (moved && gtaPos != previousGtaPos)
                            {
                                instance.position = FromGtaCoords(gtaPos);
                                OnInstanceTransformUpdated(i, true, false);
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnMove);
                            }
                            if (rotated && instance.rotation != previousRot)
                            {
                                OnInstanceTransformUpdated(i, false, true);
                                HandleAutoBuild(NavmeshAutoBuildFlag::OnRotate);
                            }

                            std::string parentLabel = "Nenhum";
                            if (instance.parentId != 0)
                            {
                                if (const MeshInstance* parent = FindMeshById(instance.parentId))
                                    parentLabel = parent->name;
                                else
                                    parentLabel = "(Parent removido)";
                            }

                            if (ImGui::BeginCombo("Parent", parentLabel.c_str()))
                            {
                                if (ImGui::Selectable("Nenhum", instance.parentId == 0))
                                {
                                    SetMeshParent(i, 0);
                                }

                                for (size_t parentIdx = 0; parentIdx < CurrentMeshes().size(); ++parentIdx)
                                {
                                    if (parentIdx == i)
                                        continue;

                                    const auto& candidate = CurrentMeshes()[parentIdx];
                                    if (WouldCreateCycle(instance.id, candidate.id))
                                        continue;

                                    bool isParent = instance.parentId == candidate.id;
                                    if (ImGui::Selectable(candidate.name.c_str(), isParent))
                                    {
                                        SetMeshParent(i, candidate.id);
                                    }
                                }

                                ImGui::EndCombo();
                            }

                            ImGui::TableNextColumn();
                            ImGui::BeginDisabled(viewportClickMode != ViewportClickMode::EditMesh);
                            if (ImGui::Button("Selecionar"))
                            {
                                CurrentPickedMeshIndex() = static_cast<int>(i);
                                CurrentPickedTri() = -1;
                            }
                            ImGui::EndDisabled();
                            ImGui::SameLine();

                            bool removeSingle = false;
                            bool removeWithChildren = false;
                            if (ImGui::Button("Remover"))
                            {
                                removeSingle = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Remover Mesh e Childs"))
                            {
                                removeWithChildren = true;
                            }

                            ImGui::PopID();
                            if (removeWithChildren)
                            {
                                uint64_t rootId = instance.id;
                                RemoveMeshSubtree(rootId);
                                if (CurrentMeshes().size() <= i)
                                    break;
                                continue;
                            }

                            if (removeSingle)
                            {
                                RemoveMesh(i);
                                if (CurrentMeshes().size() <= i)
                                    break;
                                continue;
                            }
                            ++i;
                        }

                        ImGui::EndTable();
                    }

                    if (CurrentMeshes().empty())
                    {
                        ImGui::TextDisabled("Nenhuma mesh carregada.");
                    }
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Navmesh Gen Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::BeginDisabled(navmeshBusy);
                    int buildMode = navGenSettings.mode == NavmeshBuildMode::Tiled ? 1 : 0;
                    if (ImGui::RadioButton("Navmesh normal (single)", buildMode == 0))
                    {
                        navGenSettings.mode = NavmeshBuildMode::SingleMesh;
                    }
                    if (ImGui::RadioButton("Tile based", buildMode == 1))
                    {
                        navGenSettings.mode = NavmeshBuildMode::Tiled;
                    }

                    ImGui::SeparatorText("Voxelização");
                    ImGui::DragFloat("Cell Size", &navGenSettings.cellSize, 0.01f, 0.01f, 5.0f, "%.3f");
                    ImGui::DragFloat("Cell Height", &navGenSettings.cellHeight, 0.01f, 0.01f, 5.0f, "%.3f");

                    ImGui::SeparatorText("Agente");
                    ImGui::DragFloat("Altura", &navGenSettings.agentHeight, 0.05f, 0.1f, 20.0f, "%.2f");
                    ImGui::DragFloat("Raio", &navGenSettings.agentRadius, 0.05f, 0.05f, 10.0f, "%.2f");
                    ImGui::DragFloat("Climb", &navGenSettings.agentMaxClimb, 0.05f, 0.0f, 10.0f, "%.2f");
                    ImGui::DragFloat("Slope", &navGenSettings.agentMaxSlope, 1.0f, 0.0f, 89.0f, "%.1f");

                    ImGui::SeparatorText("Polígonos e Regiões");
                    ImGui::DragFloat("Max Edge Len", &navGenSettings.maxEdgeLen, 0.5f, 0.1f, 200.0f, "%.2f");
                    ImGui::DragFloat("Simplification Error", &navGenSettings.maxSimplificationError, 0.01f, 0.0f, 10.0f, "%.2f");
                    ImGui::DragFloat("Min Region Size", &navGenSettings.minRegionSize, 0.5f, 0.0f, 200.0f, "%.1f");
                    ImGui::DragFloat("Merge Region Size", &navGenSettings.mergeRegionSize, 0.5f, 0.0f, 400.0f, "%.1f");
                    ImGui::DragInt("Max Verts/Poly", &navGenSettings.maxVertsPerPoly, 1, 3, 12);

                    ImGui::SeparatorText("Detalhamento");
                    ImGui::DragFloat("Detail Sample Dist", &navGenSettings.detailSampleDist, 0.25f, 0.0f, 100.0f, "%.2f");
                    ImGui::DragFloat("Detail Sample Max Error", &navGenSettings.detailSampleMaxError, 0.1f, 0.0f, 50.0f, "%.2f");

                    ImGui::BeginDisabled(navGenSettings.mode != NavmeshBuildMode::Tiled);
                    ImGui::DragInt("Tile Size (cells)", &navGenSettings.tileSize, 1, 1, 512);
                    ImGui::EndDisabled();

                    navGenSettings.maxVertsPerPoly = std::max(3, navGenSettings.maxVertsPerPoly);
                    navGenSettings.tileSize = std::max(1, navGenSettings.tileSize);
                    ImGui::EndDisabled();
                }

                if (ImGui::CollapsingHeader("Navmesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool hasMesh = !CurrentMeshes().empty();
                    bool hasNavmesh = hasNavmeshLoaded;
                    ImGui::Text("Meshes carregadas: %zu", CurrentMeshes().size());

                    if (navmeshBusy)
                    {
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Navmesh build em andamento... câmera e pathfind continuam ativos.");
                        if (navmeshStopRequested)
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Cancelamento requisitado. Aguardando término seguro do worker.");
                        }
                    }

                    ImGui::BeginDisabled(!hasMesh || navmeshBusy);
                    if (ImGui::Button("Build Navmesh"))
                    {
                        buildNavmeshFromMeshes(true, currentNavmeshSlot);
                    }
                    ImGui::EndDisabled();

                    ImGui::BeginDisabled(navmeshBusy || !hasNavmesh);
                    if (ImGui::Button("Clear Navmesh"))
                    {
                        ClearNavmesh();
                    }
                    ImGui::EndDisabled();

                    ImGui::BeginDisabled(!navmeshBusy);
                    if (ImGui::Button("Stop Build"))
                    {
                        RequestStopNavmeshBuild();
                    }
                    ImGui::EndDisabled();

                    ImGui::SeparatorText("Visibilidade");
                    ImGui::Checkbox("Mostrar navmesh", &navmeshVisible);
                    int navRenderMode = static_cast<int>(navmeshRenderMode);
                    ImGui::RadioButton("Faces e linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesAndLines));
                    ImGui::RadioButton("Somente faces", &navRenderMode, static_cast<int>(NavmeshRenderMode::FacesOnly));
                    ImGui::RadioButton("Somente linhas", &navRenderMode, static_cast<int>(NavmeshRenderMode::LinesOnly));
                    navmeshRenderMode = static_cast<NavmeshRenderMode>(navRenderMode);

                    ImGui::Checkbox("Mostrar faces da navmesh", &navmeshShowFaces);
                    ImGui::BeginDisabled(!navmeshShowFaces || navmeshRenderMode == NavmeshRenderMode::LinesOnly);
                    ImGui::SliderFloat("Alpha das faces", &navmeshFaceAlpha, 0.0f, 1.0f);
                    ImGui::EndDisabled();
                    ImGui::Checkbox("Mostrar linhas da navmesh", &navmeshShowLines);

                    ImGui::SeparatorText("Construção Automática");
                    ImGui::BeginDisabled(navmeshBusy);
                    bool autoOnAdd    = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd)) != 0;
                    bool autoOnRemove = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove)) != 0;
                    bool autoOnMove   = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove)) != 0;
                    bool autoOnRotate = (navmeshAutoBuildMask & static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate)) != 0;

                    if (ImGui::Checkbox("Ao adicionar Mesh", &autoOnAdd))
                    {
                        if (autoOnAdd)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnAdd);
                    }

                    if (ImGui::Checkbox("Ao remover Mesh", &autoOnRemove))
                    {
                        if (autoOnRemove)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRemove);
                    }

                    if (ImGui::Checkbox("Ao mover mesh", &autoOnMove))
                    {
                        if (autoOnMove)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnMove);
                    }

                    if (ImGui::Checkbox("Ao rotacionar mesh", &autoOnRotate))
                    {
                        if (autoOnRotate)
                            navmeshAutoBuildMask |= static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                        else
                            navmeshAutoBuildMask &= ~static_cast<uint32_t>(NavmeshAutoBuildFlag::OnRotate);
                    }
                    ImGui::EndDisabled();

                    ImGui::Separator();
                    ImGui::Text("Tris: %zu", navmeshTris.size() / 3);
                    ImGui::Text("Linhas: %zu", navmeshLineBuffer.size());
                    ImGui::Text("Offmesh Links: %zu", CurrentNavData().GetOffmeshLinks().size());
                }

                ImGui::EndTabItem();
            }

            if (showDebugInfo && ImGui::BeginTabItem("Diagnóstico"))
            {
                const MeshInstance* infoInstance = nullptr;
                if (!CurrentMeshes().empty())
                {
                    if (CurrentPickedMeshIndex() >= 0 && CurrentPickedMeshIndex() < static_cast<int>(CurrentMeshes().size()))
                        infoInstance = &CurrentMeshes()[CurrentPickedMeshIndex()];
                    else
                        infoInstance = &CurrentMeshes().front();
                }

                if (ImGui::CollapsingHeader("Mesh Info", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (infoInstance && infoInstance->mesh)
                    {
                        ImGui::Text("Mesh: %s", infoInstance->name.c_str());
                        ImGui::Text("Navmesh bounds:");
                        ImGui::Text("Min: %.1f %.1f %.1f",
                            infoInstance->mesh->navmeshMinBounds.x,
                            infoInstance->mesh->navmeshMinBounds.y,
                            infoInstance->mesh->navmeshMinBounds.z);

                        ImGui::Text("Max: %.1f %.1f %.1f",
                            infoInstance->mesh->navmeshMaxBounds.x,
                            infoInstance->mesh->navmeshMaxBounds.y,
                            infoInstance->mesh->navmeshMaxBounds.z);

                        glm::vec3 center = (infoInstance->mesh->navmeshMinBounds + infoInstance->mesh->navmeshMaxBounds) * 0.5f;
                        ImGui::Text("Center: %.1f %.1f %.1f",
                            center.x, center.y, center.z);

                        ImGui::Separator();
                        ImGui::Text("Render bounds (offset: %.1f %.1f %.1f)",
                            infoInstance->mesh->renderOffset.x,
                            infoInstance->mesh->renderOffset.y,
                            infoInstance->mesh->renderOffset.z);
                        ImGui::Text("Min: %.1f %.1f %.1f",
                            infoInstance->mesh->renderMinBounds.x,
                            infoInstance->mesh->renderMinBounds.y,
                            infoInstance->mesh->renderMinBounds.z);

                        ImGui::Text("Max: %.1f %.1f %.1f",
                            infoInstance->mesh->renderMaxBounds.x,
                            infoInstance->mesh->renderMaxBounds.y,
                            infoInstance->mesh->renderMaxBounds.z);

                        glm::vec3 renderCenter = (infoInstance->mesh->renderMinBounds + infoInstance->mesh->renderMaxBounds) * 0.5f;
                        ImGui::Text("Render Center: %.1f %.1f %.1f",
                            renderCenter.x, renderCenter.y, renderCenter.z);

                        if (ImGui::Button("Teleport Camera to Mesh Center"))
                            camera->pos = renderCenter + glm::vec3(0,50,150);

                        if (ImGui::Button("Teleport Camera to Selected Mesh"))
                            camera->pos = glm::vec3(infoInstance->position.x,infoInstance->position.y,infoInstance->position.z);
                    }
                    else
                    {
                        ImGui::TextDisabled("Nenhuma mesh selecionada.");
                    }
                }

                if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("Camera: %.2f %.2f %.2f",
                        camera->pos.x, camera->pos.y, camera->pos.z);
                    ImGui::Text("Meshes carregadas: %zu", CurrentMeshes().size());
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        propHashBrowser.Display();
        if (propHashBrowser.HasSelected())
        {
            auto selected = propHashBrowser.GetSelected();
            propHashBrowser.ClearSelected();
            memoryHandler.SetPropHashFile(selected);
        }

        objDirectoryBrowser.Display();
        if (objDirectoryBrowser.HasSelected())
        {
            auto selected = objDirectoryBrowser.GetSelected();
            objDirectoryBrowser.ClearSelected();
            memoryHandler.SetObjDirectory(selected);
        }

        ImGui::End();
    }

    // --- OpenGL ---
    glEnable(GL_DEPTH_TEST);
    glClearColor(0,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Begin renderer com renderMode
    renderer->Begin(camera, renderMode);
    glUniform3f(glGetUniformLocation(renderer->GetShader(), "uSolidColor"), 1,1,1);

    glm::vec3 defaultBaseColor = renderMode == RenderMode::Lit ?
        glm::vec3(0.8f, 0.8f, 0.9f) : glm::vec3(0.72f, 0.76f, 0.82f);

    for (size_t meshIdx = 0; meshIdx < CurrentMeshes().size(); ++meshIdx)
    {
        const auto& instance = CurrentMeshes()[meshIdx];
        if (!instance.mesh)
            continue;

        bool isSelected = viewportClickMode == ViewportClickMode::EditMesh &&
            static_cast<int>(meshIdx) == CurrentPickedMeshIndex();

        glm::mat4 model = GetModelMatrix(instance);

        glUseProgram(renderer->GetShader());
        glm::vec3 meshColor = isSelected ? glm::vec3(1.0f, 0.6f, 0.2f) : defaultBaseColor;
        glUniform3f(glGetUniformLocation(renderer->GetShader(), "uBaseColor"), meshColor.x, meshColor.y, meshColor.z);
        glUniformMatrix4fv(
            glGetUniformLocation(renderer->GetShader(), "uModel"),
            1, GL_FALSE, &model[0][0]
        );

        instance.mesh->Draw();

        if (CurrentPickedTri() >= 0 && CurrentPickedMeshIndex() == static_cast<int>(meshIdx))
        {
            if (CurrentPickedTri() + 2 >= (int)instance.mesh->indices.size())
            {
                CurrentPickedTri() = -1;
            }
            else
            {
                int i0 = instance.mesh->indices[CurrentPickedTri()+0];
                int i1 = instance.mesh->indices[CurrentPickedTri()+1];
                int i2 = instance.mesh->indices[CurrentPickedTri()+2];

                glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
                glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
                glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

                renderer->DrawTriangleHighlight(v0, v1, v2);
            }
        }
    }

    bool drawFaces = navmeshVisible && navmeshShowFaces &&
        navmeshRenderMode != NavmeshRenderMode::LinesOnly &&
        !navmeshTris.empty();
    bool drawLines = navmeshVisible && navmeshShowLines &&
        navmeshRenderMode != NavmeshRenderMode::FacesOnly &&
        !navmeshLineBuffer.empty();
    bool drawPath = !pathLines.empty();
    bool drawOffmeshLinks = !offmeshLines.empty() &&
        (navmeshVisible || viewportClickMode == ViewportClickMode::AddOffmeshLink);

    if (drawFaces)
    {
        float clampedAlpha = std::clamp(navmeshFaceAlpha, 0.0f, 1.0f);
        renderer->drawNavmeshTriangles(navmeshTris, glm::vec3(0.5f, 0.7f, 1.0f), clampedAlpha);
    }

    if (drawLines)
    {
        renderer->drawNavmeshLines(navmeshLineBuffer);
    }

    if (drawOffmeshLinks)
    {
        renderer->drawNavmeshLines(offmeshLines, glm::vec3(0.6f, 0.0f, 0.8f), 2.0f);
    }

    if (boundingBoxVisible)
    {
        if (!boundingBoxFill.empty())
        {
            renderer->drawNavmeshTriangles(boundingBoxFill, glm::vec3(1.0f, 0.0f, 0.0f), std::clamp(boundingBoxAlpha, 0.0f, 1.0f));
        }
        if (!boundingBoxLines.empty())
        {
            renderer->drawNavmeshLines(boundingBoxLines, glm::vec3(1.0f, 0.0f, 0.0f), 3.0f);
        }
    }

    if (drawPath)
    {
        renderer->drawNavmeshLines(pathLines, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    DrawSelectedMeshHighlight();

    // desenhar grid e eixo sempre no modo 99
    glDisable(GL_DEPTH_TEST);   // Gizmos ficam por cima

    renderer->Begin(camera, RenderMode::None); // vai setar modo=99
    DrawEditGizmo();
    if (boundingBoxVisible)
        DrawBoundingBoxGizmo();
    renderer->DrawGrid();
    renderer->DrawAxisGizmoScreen(camera, 1600, 900);
    renderer->End();

    glEnable(GL_DEPTH_TEST);


    renderer->End();

    // --- Finalizar ImGui ---
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}
