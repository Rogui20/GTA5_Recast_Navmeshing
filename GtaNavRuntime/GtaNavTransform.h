#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

inline void TransformVertexZYX(
    float x, float y, float z,
    const float pos[3],
    const float rotDeg[3],
    float& ox, float& oy, float& oz)
{
    // Ordem de rotação escolhida: YXZ → Z,Y,X (GTA geralmente usa ZYX)
    glm::mat4 t =
        glm::eulerAngleYXZ(
            glm::radians(rotDeg[2]), // Z
            glm::radians(rotDeg[0]), // Y
            glm::radians(rotDeg[1])  // X
        );

    glm::vec4 v(x, y, z, 1.0f);
    glm::vec4 r = t * v;

    ox = r.x + pos[0];
    oy = r.y + pos[1];
    oz = r.z + pos[2];
}
