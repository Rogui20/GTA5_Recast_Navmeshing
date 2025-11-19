#pragma once
#include <vector>
#include <glm/glm.hpp>

class Mesh
{
public:
    Mesh();
    ~Mesh();

    bool UploadToGPU();

    void Draw();

    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;


private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    bool gpuUploaded = false;
};
