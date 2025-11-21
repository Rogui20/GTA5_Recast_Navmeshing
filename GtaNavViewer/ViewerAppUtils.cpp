#include "ViewerApp.h"

#include "MathUtils.h"
#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cfloat>
#include <cmath>
#include <limits>

glm::mat4 ViewerApp::GetModelMatrix(const MeshInstance& instance) const
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, instance.position);
    model = glm::rotate(model, glm::radians(instance.rotation.x), glm::vec3(1,0,0));
    model = glm::rotate(model, glm::radians(instance.rotation.y), glm::vec3(0,1,0));
    model = glm::rotate(model, glm::radians(instance.rotation.z), glm::vec3(0,0,1));
    model = glm::scale(model, glm::vec3(instance.scale));
    return model;
}

ViewerApp::MeshBoundsState ViewerApp::ComputeMeshBounds(const MeshInstance& instance) const
{
    MeshBoundsState state;
    if (!instance.mesh || instance.mesh->renderVertices.empty())
        return state;

    glm::mat4 model = GetModelMatrix(instance);
    glm::vec3 bmin(std::numeric_limits<float>::max());
    glm::vec3 bmax(std::numeric_limits<float>::lowest());

    for (const auto& v : instance.mesh->renderVertices)
    {
        glm::vec3 world = glm::vec3(model * glm::vec4(v, 1.0f));
        bmin = glm::min(bmin, world);
        bmax = glm::max(bmax, world);
    }

    state.bmin = bmin;
    state.bmax = bmax;
    state.vertexCount = instance.mesh->renderVertices.size();
    state.indexCount = instance.mesh->indices.size();
    state.meshPtr = instance.mesh.get();
    state.valid = true;
    return state;
}

bool ViewerApp::HasMeshChanged(const MeshBoundsState& previous, const MeshBoundsState& current) const
{
    if (!previous.valid || !current.valid)
        return true;

    if (previous.meshPtr != current.meshPtr ||
        previous.vertexCount != current.vertexCount ||
        previous.indexCount != current.indexCount)
    {
        return true;
    }

    const float eps = 1e-3f;
    auto differentVec3 = [&](const glm::vec3& a, const glm::vec3& b)
    {
        return fabsf(a.x - b.x) > eps || fabsf(a.y - b.y) > eps || fabsf(a.z - b.z) > eps;
    };

    return differentVec3(previous.bmin, current.bmin) || differentVec3(previous.bmax, current.bmax);
}

void ViewerApp::ResetPathState()
{
    hasPathStart = false;
    hasPathTarget = false;
    pathLines.clear();
}

bool ViewerApp::InitNavQueryForCurrentNavmesh()
{
    navQueryReady = false;

    if (!navData.IsLoaded())
        return false;

    if (!navQuery)
    {
        navQuery = dtAllocNavMeshQuery();
        if (!navQuery)
        {
            printf("[ViewerApp] InitNavQueryForCurrentNavmesh: dtAllocNavMeshQuery falhou.\n");
            return false;
        }
    }

    const dtStatus status = navQuery->init(navData.GetNavMesh(), 2048);
    if (dtStatusFailed(status))
    {
        printf("[ViewerApp] InitNavQueryForCurrentNavmesh: navQuery->init falhou.\n");
        return false;
    }

    navQueryReady = true;
    return true;
}

bool ViewerApp::ComputeRayMeshHit(int mx, int my, glm::vec3& outPoint, int* outTri, int* outMeshIndex)
{
    int screenW = 1600;
    int screenH = 900;
    SDL_GetWindowSize(window, &screenW, &screenH);

    Ray ray = camera->GetRayFromScreen(mx, my, screenW, screenH);

    float best = FLT_MAX;
    bool hasHit = false;
    if (outTri)
        *outTri = -1;
    if (outMeshIndex)
        *outMeshIndex = -1;

    for (size_t meshIdx = 0; meshIdx < meshInstances.size(); ++meshIdx)
    {
        const auto& instance = meshInstances[meshIdx];
        if (!instance.mesh)
            continue;

        glm::mat4 model = GetModelMatrix(instance);
        for (int i = 0; i < instance.mesh->indices.size(); i += 3)
        {
            int i0 = instance.mesh->indices[i+0];
            int i1 = instance.mesh->indices[i+1];
            int i2 = instance.mesh->indices[i+2];

            glm::vec3 v0 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i0], 1.0f));
            glm::vec3 v1 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i1], 1.0f));
            glm::vec3 v2 = glm::vec3(model * glm::vec4(instance.mesh->renderVertices[i2], 1.0f));

            float dist;
            if (RayTriangleIntersect(ray, v0, v1, v2, dist))
            {
                if (dist < best)
                {
                    best = dist;
                    outPoint = ray.origin + ray.dir * dist;
                    if (outTri)
                        *outTri = i;
                    if (outMeshIndex)
                        *outMeshIndex = static_cast<int>(meshIdx);
                    hasHit = true;
                }
            }
        }
    }

    return hasHit;
}
