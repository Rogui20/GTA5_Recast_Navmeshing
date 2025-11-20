#pragma once
#include <vector>
#include <glm/glm.hpp>

class dtNavMesh;

enum class NavmeshBuildMode
{
    SingleMesh = 0,
    Tiled
};

struct NavmeshGenerationSettings
{
    NavmeshBuildMode mode = NavmeshBuildMode::SingleMesh;
    float cellSize = 0.3f;
    float cellHeight = 0.2f;
    float agentHeight = 2.0f;
    float agentRadius = 0.6f;
    float agentMaxClimb = 1.0f;
    float agentMaxSlope = 60.0f;
    float maxEdgeLen = 12.0f;
    float maxSimplificationError = 1.3f;
    float minRegionSize = 8.0f;
    float mergeRegionSize = 20.0f;
    int maxVertsPerPoly = 6;
    float detailSampleDist = 6.0f;
    float detailSampleMaxError = 1.0f;
    int tileSize = 48;
};

class NavMeshData
{
public:
    ~NavMeshData();
    bool Load(const char* path);
    bool IsLoaded() const { return m_nav != nullptr; }
    // NOVO: constrói navmesh direto da malha
    bool BuildFromMesh(const std::vector<glm::vec3>& verts,
                       const std::vector<unsigned int>& indices,
                       const NavmeshGenerationSettings& settings);

                       
    // Conversão para desenhar no viewer
    void ExtractDebugMesh(
        std::vector<glm::vec3>& outVerts,
        std::vector<glm::vec3>& outLines
    );

private:
    dtNavMesh* m_nav = nullptr;
};
