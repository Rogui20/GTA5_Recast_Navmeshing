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
    // Offset aplicado aos vertices de renderização (usado para reposicionar no mundo)
    glm::vec3 renderOffset = glm::vec3(0.0f);
    glm::vec3 renderMinBounds;
    glm::vec3 renderMaxBounds;

    // Vertices absolutos originais (usados para construir navmesh)
    std::vector<glm::vec3> navmeshVertices;
    std::vector<unsigned int> indices;
    glm::vec3 navmeshMinBounds;
    glm::vec3 navmeshMaxBounds;


private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    bool gpuUploaded = false;
};
