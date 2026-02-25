#include "ViewerApp.h"

#include <cstdio>

bool ViewerApp::IsPathfindModeActive() const
{
    return viewportClickMode == ViewportClickMode::Pathfind_Normal || viewportClickMode == ViewportClickMode::Pathfind_MinEdge;
}

void ViewerApp::TryRunPathfind()
{
    auto& pathLines = CurrentPathLines();
    pathLines.clear();

    if (!IsPathfindModeActive())
        return;

    if (!CurrentHasPathStart() || !CurrentHasPathTarget())
        return;

    if (!CurrentNavData().IsLoaded())
    {
        printf("[ViewerApp] TryRunPathfind: navmesh nao carregado.\n");
        return;
    }

    if (!CurrentNavQueryReady() && !InitNavQueryForCurrentNavmesh())
    {
        printf("[ViewerApp] TryRunPathfind: navmesh/query indisponivel.\n");
        return;
    }

    const glm::vec3& pathStart = CurrentPathStart();
    const glm::vec3& pathTarget = CurrentPathTarget();
    const float startPos[3] = { pathStart.x, pathStart.y, pathStart.z };
    const float endPos[3]   = { pathTarget.x, pathTarget.y, pathTarget.z };
    const float extents[3]  = { navGenSettings.agentRadius * 4.0f + 0.1f, navGenSettings.agentHeight * 0.5f + 0.1f, navGenSettings.agentRadius * 4.0f + 0.1f };

    dtPolyRef startRef = 0, endRef = 0;
    float startNearest[3]{};
    float endNearest[3]{};

    if (dtStatusFailed(CurrentNavQuery()->findNearestPoly(startPos, extents, &pathQueryFilter, &startRef, startNearest)) || startRef == 0)
    {
        printf("[ViewerApp] TryRunPathfind: nao achou poly inicial pro start.\n");
        return;
    }

    if (dtStatusFailed(CurrentNavQuery()->findNearestPoly(endPos, extents, &pathQueryFilter, &endRef, endNearest)) || endRef == 0)
    {
        printf("[ViewerApp] TryRunPathfind: nao achou poly final pro target.\n");
        return;
    }

    dtPolyRef polys[256];
    int polyCount = 0;
    const dtStatus pathStatus = CurrentNavQuery()->findPath(startRef, endRef, startNearest, endNearest, &pathQueryFilter, polys, &polyCount, 256);
    if (dtStatusFailed(pathStatus) || polyCount == 0)
    {
        printf("[ViewerApp] TryRunPathfind: findPath falhou ou nenhum poly no caminho.\n");
        return;
    }

    float straight[256 * 3];
    unsigned char straightFlags[256];
    dtPolyRef straightRefs[256];
    int straightCount = 0;

    dtStatus straightStatus = DT_FAILURE;
    if (viewportClickMode == ViewportClickMode::Pathfind_Normal)
    {
        straightStatus = CurrentNavQuery()->findStraightPath(startNearest, endNearest, polys, polyCount, straight, straightFlags, straightRefs, &straightCount, 256, 2);
    }
    else if (viewportClickMode == ViewportClickMode::Pathfind_MinEdge)
    {
        straightStatus = CurrentNavQuery()->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, straight, straightFlags, straightRefs, &straightCount, 256, 2, pathfindMinEdgeDistance);
    }

    if (dtStatusFailed(straightStatus) || straightCount == 0)
    {
        printf("[ViewerApp] TryRunPathfind: findStraightPath falhou.\n");
        return;
    }

    for (int i = 0; i + 1 < straightCount; ++i)
    {
        DebugLine dl{};
        dl.x0 = straight[i * 3 + 0];
        dl.y0 = straight[i * 3 + 1];
        dl.z0 = straight[i * 3 + 2];

        dl.x1 = straight[(i + 1) * 3 + 0];
        dl.y1 = straight[(i + 1) * 3 + 1];
        dl.z1 = straight[(i + 1) * 3 + 2];
        pathLines.push_back(dl);
    }

    printf("[ViewerApp] Pathfind gerou %d pontos (%zu segmentos).\n", straightCount, pathLines.size());
}