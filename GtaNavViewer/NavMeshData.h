#pragma once
#include <utility>
#include <vector>
#include <atomic>
#include <glm/glm.hpp>
#include <Recast.h>

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

struct OffmeshLink
{
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    float radius = 1.0f;
    bool bidirectional = true;
    unsigned char area = RC_WALKABLE_AREA;
    unsigned short flags = 1;
    unsigned int userId = 0;
    int ownerTx = -1;
    int ownerTy = -1;
};

struct AutoOffmeshGenerationParams
{
    int linksGenFlags = 1;      // bit 0 = drop/jump, bit 1 = facing normals
    float jumpHeight = 2.0f;
    float maxDropHeight = 3.0f;
    float maxSlopeDegrees = 60.0f;
    float agentRadius = 0.6f;
    float agentHeight = 2.0f;
    unsigned int userIdBase = 0xAFAF0000u;
    float edgeOutset = 0.15f;
    float upOffset = 0.10f;
    float raycastExtraHeight = 0.5f;
    float minDropThreshold = 0.20f;
    float minNeighborHeightDelta = 0.30f;
    unsigned char dropArea = 5; // segue convenção do RecastDemo (jump area)
    float angleTolerance = 30.0f;
    float maxHeightDiff = 1.5f;
    float minHeightDiff = 1.0f;
    float minDistance = 0.30f;
    float maxDistance = 5.0f;
    float normalOffset = 0.10f;
    float zOffset = 0.05f;
};

enum NavAreas : unsigned char
{
    AREA_NULL     = 0,
    AREA_GROUND   = RC_WALKABLE_AREA, // 63
    AREA_OFFMESH  = 2,
    AREA_JUMP  = 3,
    AREA_DROP  = 4
};

class NavMeshData
{
public:
    NavMeshData() = default;
    NavMeshData(const NavMeshData&) = delete;
    NavMeshData& operator=(const NavMeshData&) = delete;
    NavMeshData(NavMeshData&& other) noexcept;
    NavMeshData& operator=(NavMeshData&& other) noexcept;
    ~NavMeshData();
    bool Load(const char* path);
    bool IsLoaded() const { return m_nav != nullptr; }
    // NOVO: constrói navmesh direto da malha
    bool BuildFromMesh(const std::vector<glm::vec3>& verts,
                       const std::vector<unsigned int>& indices,
                       const NavmeshGenerationSettings& settings,
                       bool buildTilesNow = true,
                       const std::atomic_bool* cancelFlag = nullptr,
                       bool useCache = true,
                       const char* cachePath = nullptr);

    bool BuildTileAt(const glm::vec3& worldPos,
                     const NavmeshGenerationSettings& settings,
                     int& outTileX,
                     int& outTileY);

    bool CollectTilesInBounds(const glm::vec3& bmin,
                              const glm::vec3& bmax,
                              bool onlyExistingTiles,
                              std::vector<std::pair<int, int>>& outTiles) const;

    bool RebuildTilesInBounds(const glm::vec3& bmin,
                              const glm::vec3& bmax,
                              const NavmeshGenerationSettings& settings,
                              bool onlyExistingTiles,
                              std::vector<std::pair<int, int>>* outTiles = nullptr);

    bool RebuildSpecificTiles(const std::vector<std::pair<int, int>>& tiles,
                              const NavmeshGenerationSettings& settings,
                              bool onlyExistingTiles,
                              std::vector<std::pair<int, int>>* outTiles = nullptr);

    bool HasTiledCache() const { return m_hasTiledCache; }
    bool UpdateCachedGeometry(const std::vector<glm::vec3>& verts,
                              const std::vector<unsigned int>& indices);

    bool RemoveTileAt(const glm::vec3& worldPos,
                      int& outTileX,
                      int& outTileY);

    dtNavMesh* GetNavMesh() const { return m_nav; }

    void AddOffmeshLink(const glm::vec3& start,
                        const glm::vec3& end,
                        float radius,
                        bool bidirectional);
    bool RemoveNearestOffmeshLink(const glm::vec3& point);
    void SetOffmeshLinks(std::vector<OffmeshLink> links);
    const std::vector<OffmeshLink>& GetOffmeshLinks() const { return m_offmeshLinks; }
    void ClearOffmeshLinks();
    bool GenerateAutomaticOffmeshLinks(const AutoOffmeshGenerationParams& params,
                                       std::vector<OffmeshLink>& outLinks) const;


    // Conversão para desenhar no viewer
    void ExtractDebugMesh(
        std::vector<glm::vec3>& outVerts,
        std::vector<glm::vec3>& outLines
    );

private:
    dtNavMesh* m_nav = nullptr;

    std::vector<float> m_cachedVerts;
    std::vector<int>   m_cachedTris;
    float m_cachedBMin[3] = {0,0,0};
    float m_cachedBMax[3] = {0,0,0};
    rcConfig m_cachedBaseCfg{};
    NavmeshGenerationSettings m_cachedSettings{};
    int m_cachedTileWidthCount = 0;
    int m_cachedTileHeightCount = 0;
    bool m_hasTiledCache = false;
    std::vector<OffmeshLink> m_offmeshLinks;
};
