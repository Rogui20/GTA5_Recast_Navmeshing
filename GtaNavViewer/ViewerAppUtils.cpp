#include "ViewerApp.h"

#include "MathUtils.h"
#include <SDL.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <cfloat>
#include <cmath>
#include <limits>
#include <array>

namespace
{
float NormalizeAngle(float angle)
{
    float wrapped = std::fmod(angle + 180.0f, 360.0f);
    if (wrapped < 0.0f)
        wrapped += 360.0f;
    return wrapped - 180.0f;
}

bool ClosestPointBetweenLines(const Ray& ray, const glm::vec3& origin, const glm::vec3& axisDir, float& outAxisParam, float& outRayParam)
{
    const float EPS = 1e-5f;
    float a = glm::dot(axisDir, axisDir);
    float b = glm::dot(axisDir, ray.dir);
    float c = glm::dot(ray.dir, ray.dir);
    glm::vec3 w0 = origin - ray.origin;
    float d = glm::dot(axisDir, w0);
    float e = glm::dot(ray.dir, w0);
    float denom = a * c - b * b;
    if (std::fabs(denom) < EPS)
        return false;

    outAxisParam = (b * e - c * d) / denom;
    outRayParam  = (a * e - b * d) / denom;
    return outRayParam >= 0.0f;
}

bool ProjectRayOntoPlane(const Ray& ray, const glm::vec3& planeOrigin, const glm::vec3& planeNormal, glm::vec3& outPoint)
{
    const float EPS = 1e-5f;
    float denom = glm::dot(planeNormal, ray.dir);
    if (std::fabs(denom) < EPS)
        return false;

    float t = glm::dot(planeOrigin - ray.origin, planeNormal) / denom;
    if (t < 0.0f)
        return false;

    outPoint = ray.origin + ray.dir * t;
    return true;
}
}

glm::mat4 ViewerApp::GetModelMatrix(const MeshInstance& instance) const
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, instance.position);
    glm::mat3 rot = GetRotationMatrix(instance.rotation);
    model = model * glm::mat4(rot);
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

glm::vec3 ViewerApp::NormalizeEuler(glm::vec3 angles) const
{
    angles.x = NormalizeAngle(angles.x);
    angles.y = NormalizeAngle(angles.y);
    angles.z = NormalizeAngle(angles.z);
    return angles;
}

void ViewerApp::NormalizeMeshRotation(MeshInstance& instance) const
{
    instance.rotation = NormalizeEuler(instance.rotation);
}

glm::vec3 ViewerApp::ToGtaCoords(const glm::vec3& internal) const
{
    return glm::vec3(internal.x, internal.z, -internal.y);
}

glm::vec3 ViewerApp::FromGtaCoords(const glm::vec3& gta) const
{
    return glm::vec3(gta.x, -gta.z, gta.y);
}

glm::mat3 ViewerApp::GetRotationMatrix(const glm::vec3& eulerDegrees) const
{
    glm::vec3 normalized = NormalizeEuler(eulerDegrees);
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, glm::radians(normalized.z), glm::vec3(0,1,0));
    rot = glm::rotate(rot, glm::radians(normalized.x), glm::vec3(1,0,0));
    rot = glm::rotate(rot, glm::radians(normalized.y), glm::vec3(0,0,1));
    return glm::mat3(rot);
}

glm::vec3 ViewerApp::GetAxisDirection(const MeshInstance& instance, GizmoAxis axis) const
{
    glm::mat3 rot = GetRotationMatrix(instance.rotation);
    switch (axis)
    {
    case GizmoAxis::X: return glm::normalize(rot * glm::vec3(1,0,0));
    case GizmoAxis::Y: return glm::normalize(rot * glm::vec3(0,1,0));
    case GizmoAxis::Z: return glm::normalize(rot * glm::vec3(0,0,1));
    default: return glm::vec3(1,0,0);
    }
}

glm::vec3 ViewerApp::GetRotationAxisDirection(const MeshInstance& instance, GizmoAxis axis) const
{
    if (axis == GizmoAxis::Z)
        return glm::vec3(0,1,0);

    return GetAxisDirection(instance, axis);
}

bool ViewerApp::TryBeginMoveDrag(const Ray& ray, MeshInstance& instance)
{
    float camDist = glm::length(camera->pos - instance.position);
    float axisLength = 4.0f + camDist * 0.05f;
    float threshold = 0.35f + camDist * 0.01f;

    auto axisDirection = [&](GizmoAxis axis)
    {
        if (moveTransformMode == MoveTransformMode::LocalTransform)
            return GetAxisDirection(instance, axis);

        switch (axis)
        {
        case GizmoAxis::X: return glm::vec3(1,0,0);
        case GizmoAxis::Y: return glm::vec3(0,1,0);
        case GizmoAxis::Z: return glm::vec3(0,0,1);
        default: return glm::vec3(1,0,0);
        }
    };

    std::array<GizmoAxis, 3> axes = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
    for (auto axis : axes)
    {
        glm::vec3 dir = glm::normalize(axisDirection(axis));

        float axisParam = 0.0f;
        float rayParam = 0.0f;
        if (!ClosestPointBetweenLines(ray, instance.position, dir, axisParam, rayParam))
            continue;

        glm::vec3 axisPoint = instance.position + dir * axisParam;
        glm::vec3 rayPoint = ray.origin + ray.dir * rayParam;
        float distance = glm::length(axisPoint - rayPoint);
        if (distance > threshold)
            continue;

        if (axisParam < 0.0f || axisParam > axisLength)
            continue;

        activeGizmoAxis = axis;
        gizmoAxisDir = dir;
        gizmoOrigin = instance.position;
        meshStartPosition = instance.position;
        gizmoStartParam = axisParam;
        gizmoGrabStart = axisPoint;
        editDragActive = true;
        return true;
    }

    return false;
}

bool ViewerApp::TryBeginRotateDrag(const Ray& ray, MeshInstance& instance)
{
    float camDist = glm::length(camera->pos - instance.position);
    float radius = 3.5f + camDist * 0.03f;
    float thickness = 0.35f + camDist * 0.01f;

    std::array<GizmoAxis, 3> axes = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
    for (auto axis : axes)
    {
        glm::vec3 axisDir = GetRotationAxisDirection(instance, axis);
        glm::vec3 hitPoint;
        if (!ProjectRayOntoPlane(ray, instance.position, axisDir, hitPoint))
            continue;

        glm::vec3 offset = hitPoint - instance.position;
        float len = glm::length(offset);
        if (len < 1e-4f)
            continue;

        if (std::fabs(len - radius) > thickness)
            continue;

        rotationStartVec = glm::normalize(offset);
        rotationStartAngles = instance.rotation;
        activeGizmoAxis = axis;
        gizmoAxisDir = axisDir;
        gizmoOrigin = instance.position;
        editDragActive = true;
        return true;
    }

    return false;
}

void ViewerApp::UpdateEditDrag(int mouseX, int mouseY)
{
    if (!editDragActive || activeGizmoAxis == GizmoAxis::None)
        return;

    if (pickedMeshIndex < 0 || pickedMeshIndex >= static_cast<int>(meshInstances.size()))
        return;

    MeshInstance& instance = meshInstances[pickedMeshIndex];
    int screenW = 1600;
    int screenH = 900;
    SDL_GetWindowSize(window, &screenW, &screenH);
    Ray ray = camera->GetRayFromScreen(mouseX, mouseY, screenW, screenH);

    if (meshEditMode == MeshEditMode::Move)
    {
        float axisParam = 0.0f;
        float rayParam = 0.0f;
        if (!ClosestPointBetweenLines(ray, gizmoOrigin, gizmoAxisDir, axisParam, rayParam))
            return;

        glm::vec3 newPos = meshStartPosition + gizmoAxisDir * (axisParam - gizmoStartParam);
        if (newPos != instance.position)
        {
            instance.position = newPos;
            HandleAutoBuild(NavmeshAutoBuildFlag::OnMove);
        }
    }
    else if (meshEditMode == MeshEditMode::Rotate)
    {
        glm::vec3 hitPoint;
        if (!ProjectRayOntoPlane(ray, gizmoOrigin, gizmoAxisDir, hitPoint))
            return;

        glm::vec3 dir = hitPoint - gizmoOrigin;
        if (glm::length(dir) < 1e-4f)
            return;

        dir = glm::normalize(dir);
        float angle = glm::degrees(glm::orientedAngle(rotationStartVec, dir, gizmoAxisDir));
        glm::vec3 newAngles = rotationStartAngles;

        switch (activeGizmoAxis)
        {
        case GizmoAxis::X: newAngles.x += angle; break;
        case GizmoAxis::Y: newAngles.y += angle; break;
        case GizmoAxis::Z: newAngles.z += angle; break;
        default: break;
        }

        newAngles = NormalizeEuler(newAngles);
        if (newAngles != instance.rotation)
        {
            instance.rotation = newAngles;
            HandleAutoBuild(NavmeshAutoBuildFlag::OnRotate);
        }
    }
}
