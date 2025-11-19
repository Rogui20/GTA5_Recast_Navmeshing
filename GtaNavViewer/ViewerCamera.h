// ViewerCamera.h
#pragma once
#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;
};

class ViewerCamera
{
    
public:
    ViewerCamera();

    void Update(const unsigned char* keys);
    void OnMouseMove(int dx, int dy);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjMatrix() const;   // <- AGORA combina com o .cpp

    Ray GetRayFromScreen(int mouseX, int mouseY, int screenW, int screenH);

public:
    glm::vec3 pos;
    float yaw;
    float pitch;
    float speed;
};
