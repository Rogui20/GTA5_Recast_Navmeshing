// ViewerCamera.cpp
#include "ViewerCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>

ViewerCamera::ViewerCamera()
{
    pos = { 0.0f, 5.0f, 10.0f };
    yaw = -90.0f;
    pitch = -10.0f;
    speed = 0.12f;
}

void ViewerCamera::OnMouseMove(int dx, int dy)
{
    float sens = 0.1f;
    yaw   += dx * sens;
    pitch -= dy * sens;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void ViewerCamera::Update(const unsigned char* keys)
{
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front   = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0,1,0)));

    if (keys[SDL_SCANCODE_W]) pos += front * speed;
    if (keys[SDL_SCANCODE_S]) pos -= front * speed;

    if (keys[SDL_SCANCODE_A]) pos -= right * speed;
    if (keys[SDL_SCANCODE_D]) pos += right * speed;

    if (keys[SDL_SCANCODE_LSHIFT]) 
        speed = 0.35f;
    else 
        speed = 0.12f;
}

glm::mat4 ViewerCamera::GetProjMatrix() const
{
    return glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 5000.0f);
}

glm::mat4 ViewerCamera::GetViewMatrix() const
{
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    return glm::lookAt(
        pos,
        pos + glm::normalize(forward),
        glm::vec3(0, 1, 0)
    );
}


Ray ViewerCamera::GetRayFromScreen(int mouseX, int mouseY, int screenW, int screenH)
{
    // 1) Convert Mouse → NDC
    float x = (2.0f * mouseX) / screenW - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenH;

    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    // 2) Clip → Eye
    glm::vec4 rayEye = glm::inverse(GetProjMatrix()) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // 3) Eye → World
    glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(GetViewMatrix()) * rayEye));

    Ray r;
    r.origin = pos;
    r.dir = rayWorld;
    return r;
}
