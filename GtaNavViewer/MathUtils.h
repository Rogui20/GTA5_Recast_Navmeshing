#pragma once

#include <glm/glm.hpp>
#include "ViewerCamera.h"

bool RayTriangleIntersect(const Ray& r,
                          const glm::vec3& v0,
                          const glm::vec3& v1,
                          const glm::vec3& v2,
                          float& outDist);
