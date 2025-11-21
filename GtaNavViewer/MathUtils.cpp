#include "MathUtils.h"

#include <cmath>

bool RayTriangleIntersect(const Ray& r,
                          const glm::vec3& v0,
                          const glm::vec3& v1,
                          const glm::vec3& v2,
                          float& outDist)
{
    const float EPS = 0.00001f;
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;

    glm::vec3 p = glm::cross(r.dir, e2);
    float det = glm::dot(e1, p);

    if (fabs(det) < EPS) return false;

    float inv = 1.0f / det;

    glm::vec3 t = r.origin - v0;

    float u = glm::dot(t, p) * inv;
    if (u < 0 || u > 1) return false;

    glm::vec3 q = glm::cross(t, e1);

    float v = glm::dot(r.dir, q) * inv;
    if (v < 0 || u + v > 1) return false;

    float dist = glm::dot(e2, q) * inv;
    if (dist < 0) return false;

    outDist = dist;
    return true;
}
