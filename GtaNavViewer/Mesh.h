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

    // Vertices usados na GPU (podem estar recentralizados para renderização)
    std::vector<glm::vec3> renderVertices;

    // Vertices absolutos originais (usados para construir navmesh)
    std::vector<glm::vec3> navmeshVertices;
    std::vector<unsigned int> indices;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;


private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    bool gpuUploaded = false;
};
