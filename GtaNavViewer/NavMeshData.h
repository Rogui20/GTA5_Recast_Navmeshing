#pragma once
#include <vector>
#include <glm/glm.hpp>

class dtNavMesh;

class NavMeshData
{
public:
    bool Load(const char* path);
    bool IsLoaded() const { return m_nav != nullptr; }
    // NOVO: constrói navmesh direto da malha
    bool BuildFromMesh(const std::vector<glm::vec3>& verts,
                       const std::vector<unsigned int>& indices);

                       
    // Conversão para desenhar no viewer
    void ExtractDebugMesh(
        std::vector<glm::vec3>& outVerts,
        std::vector<glm::vec3>& outLines
    );

private:
    dtNavMesh* m_nav = nullptr;
};
