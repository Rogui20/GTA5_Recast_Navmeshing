#include "ViewerApp.h"
#include "ViewerCamera.h"

#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>

#include <cmath>

void ViewerApp::ProcessEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        ImGuiIO& io = ImGui::GetIO();
        const bool mouseOnUI = io.WantCaptureMouse;
        const bool navmeshBusy = IsNavmeshJobRunning();

        if (e.type == SDL_QUIT)
            running = false;

        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            running = false;

        if (e.type == SDL_KEYDOWN)
        {
            switch (e.key.keysym.sym)
            {
            case SDLK_F1:
                renderMode = RenderMode::Solid;
                break;

            case SDLK_F2:
                renderMode = RenderMode::Wireframe;
                break;

            case SDLK_F3:
                renderMode = RenderMode::Normals;
                break;

            case SDLK_F4:
                renderMode = RenderMode::Depth;
                break;
            case SDLK_F5:
                renderMode = RenderMode::Lit;
                break;

            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            leftMouseDown = true;
            if (mouseOnUI)
                continue;

            if (meshInstances.empty())
                continue;

            if (navmeshBusy && !IsPathfindModeActive())
            {
                printf("[ViewerApp] Operações de clique bloqueadas enquanto o navmesh é gerado.\n");
                continue;
            }

            int mx = e.button.x;
            int my = e.button.y;

            if (viewportClickMode == ViewportClickMode::PickTriangle)
            {
                pickedTri = -1;
                pickedMeshIndex = -1;
            }

            if (viewportClickMode == ViewportClickMode::EditMesh)
            {
                int screenW = 1600;
                int screenH = 900;
                SDL_GetWindowSize(window, &screenW, &screenH);
                Ray ray = camera->GetRayFromScreen(mx, my, screenW, screenH);

                if (pickedMeshIndex >= 0 && pickedMeshIndex < static_cast<int>(meshInstances.size()))
                {
                    auto& instance = meshInstances[pickedMeshIndex];
                    if (meshEditMode == MeshEditMode::Move && TryBeginMoveDrag(ray, instance))
                        continue;
                    if (meshEditMode == MeshEditMode::Rotate && TryBeginRotateDrag(ray, instance))
                        continue;
                }

                glm::vec3 hitPoint(0.0f);
                int hitTri = -1;
                int hitMesh = -1;
                if (ComputeRayMeshHit(mx, my, hitPoint, &hitTri, &hitMesh))
                {
                    pickedTri = hitTri;
                    pickedMeshIndex = hitMesh;
                }
                continue;
            }

            glm::vec3 hitPoint(0.0f);
            bool hasHit = ComputeRayMeshHit(mx, my, hitPoint,
                                            viewportClickMode == ViewportClickMode::PickTriangle ? &pickedTri : nullptr,
                                            viewportClickMode == ViewportClickMode::PickTriangle ? &pickedMeshIndex : nullptr);

            if (!hasHit)
                continue;

            switch (viewportClickMode)
            {
            case ViewportClickMode::PickTriangle:
                break;
            case ViewportClickMode::BuildTileAt:
            {
                int tileX = -1;
                int tileY = -1;
                if (navData.BuildTileAt(hitPoint, navGenSettings, tileX, tileY))
                {
                    ResetPathState();
                    buildNavmeshDebugLines();
                }
                else
                {
                    printf("[ViewerApp] BuildTileAt falhou para o clique em (%.2f, %.2f, %.2f).\\n",
                           hitPoint.x, hitPoint.y, hitPoint.z);
                }
                break;
            }
            case ViewportClickMode::RemoveTileAt:
            {
                int tileX = -1;
                int tileY = -1;
                if (navData.RemoveTileAt(hitPoint, tileX, tileY))
                {
                    ResetPathState();
                    buildNavmeshDebugLines();
                }
                break;
            }
            case ViewportClickMode::Pathfind_Normal:
            case ViewportClickMode::Pathfind_MinEdge:
                pathTarget = hitPoint;
                hasPathTarget = true;
                TryRunPathfind();
                break;
            case ViewportClickMode::EditMesh:
                break;
            case ViewportClickMode::AddOffmeshLink:
                if (!hasOffmeshStart)
                {
                    offmeshStart = hitPoint;
                    hasOffmeshStart = true;
                }
                else
                {
                    offmeshTarget = hitPoint;
                    hasOffmeshTarget = true;
                    navData.AddOffmeshLink(offmeshStart, offmeshTarget, navGenSettings.agentRadius, offmeshBidirectional);
                    RebuildOffmeshLinkLines();
                    printf("[ViewerApp] Offmesh link adicionado. Start=(%.2f, %.2f, %.2f) Target=(%.2f, %.2f, %.2f) BiDir=%s\n",
                           offmeshStart.x, offmeshStart.y, offmeshStart.z,
                           offmeshTarget.x, offmeshTarget.y, offmeshTarget.z,
                           offmeshBidirectional ? "true" : "false");
                    hasOffmeshStart = false;
                    hasOffmeshTarget = false;

                    if (navData.IsLoaded())
                    {
                        buildNavmeshFromMeshes();
                    }
                }
                break;
            }
        }

        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
        {
            leftMouseDown = false;
            editDragActive = false;
            activeGizmoAxis = GizmoAxis::None;
        }



        // Mouse capture toggle
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
        {
            if (mouseOnUI)
                continue;

            rightButtonDown = true;
            rightButtonDragged = false;
            rightButtonDownX = e.button.x;
            rightButtonDownY = e.button.y;

            mouseCaptured = true;
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        if (e.type == SDL_MOUSEMOTION)
        {
            if (rightButtonDown && (std::abs(e.motion.xrel) > 2 || std::abs(e.motion.yrel) > 2))
            {
                rightButtonDragged = true;
            }

            if (mouseCaptured)
            {
                camera->OnMouseMove(e.motion.xrel, e.motion.yrel);
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT)
        {
            mouseCaptured = false;
            SDL_SetRelativeMouseMode(SDL_FALSE);

            const bool shouldHandleClick = rightButtonDown && !rightButtonDragged;
            rightButtonDown = false;

            if (mouseOnUI)
                continue;

            if (shouldHandleClick && IsPathfindModeActive() && !meshInstances.empty())
            {
                glm::vec3 hitPoint(0.0f);
                if (ComputeRayMeshHit(rightButtonDownX, rightButtonDownY, hitPoint, nullptr, nullptr))
                {
                    pathStart = hitPoint;
                    hasPathStart = true;
                    TryRunPathfind();
                }
            }

            rightButtonDragged = false;
        }
    }

    const Uint8* state = SDL_GetKeyboardState(NULL);
    camera->Update(state);
}
