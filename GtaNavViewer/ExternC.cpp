#include "ExternC.h"
#include "NavMesh_TileCacheDB.h"
#include "json.hpp"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <iostream>

namespace
{
    inline uint64_t MakeGridCellKey(int cx, int cz)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cz);
    }

    struct LoadedGeometry
    {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
        glm::vec3 bmin{FLT_MAX};
        glm::vec3 bmax{-FLT_MAX};
        bool Valid() const { return !vertices.empty() && !indices.empty(); }
    };

    struct GeometryInstance
    {
        std::string id;
        LoadedGeometry source;
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f}; // graus
        glm::vec3 worldBMin{0.0f};
        glm::vec3 worldBMax{0.0f};
    };

    enum : std::uint8_t
    {
        SIM_FRAMEFLAG_JUMP = 1u << 0,
        SIM_FRAMEFLAG_OFFMESH_SOON = 1u << 1,
        SIM_FRAMEFLAG_NEEDS_REPATH = 1u << 2,
        SIM_FRAMEFLAG_STUCK = 1u << 3,
        SIM_FRAMEFLAG_AIRBORNE = 1u << 4,
        SIM_FRAMEFLAG_GROUND_FIT_OK = 1u << 5,
        SIM_FRAMEFLAG_GROUND_FIT_PARTIAL = 1u << 6,
        SIM_FRAMEFLAG_OFFMESH_TRAVERSAL = 1u << 7,
    };

    struct HeightSampler
    {
        bool enabled = false;
        bool built = false;
        int samplesPerTile = 64;
        bool storeTwoLayers = true;
    };

    struct SimAgentState
    {
        std::uint32_t id = 0;
        SimShapeType shape = SHAPE_CYLINDER;
        std::uint32_t teamMask = 0;
        std::uint32_t avoidMask = 0;
        std::uint32_t flags = 0;
        glm::vec3 pos{0.0f};
        float headingDeg = 0.0f;
        float radius = 0.5f;
        float halfX = 0.5f;
        float halfZ = 0.5f;
        float height = 1.8f;
        std::vector<float> cornersXYZ;
        std::vector<unsigned char> cornerFlags;
        int cornerCount = 0;
        int cornerIndex = 0;
        std::vector<dtPolyRef> pathPolys;
        dtPolyRef currentRef = 0;
        float verticalVel = 0.0f;
        glm::vec3 lastVel{0.0f};
        glm::vec3 eulerRPYDeg{0.0f};
        float rollDeg = 0.0f;
        float pitchDeg = 0.0f;
        int moveSurfaceFailCount = 0;
        bool inOffmesh = false;
        glm::vec3 offStart{0.0f};
        glm::vec3 offEnd{0.0f};
        float offT = 0.0f;
        float offDuration = 0.0f;
        std::uint8_t offType = 0;
        float offArcHeight = 0.0f;
        int offmeshStartCornerIndex = -1;
    };

    struct DynObstacleState
    {
        uint32_t id = 0;
        uint32_t teamMask = 0;
        uint32_t avoidMask = 0;
        uint8_t shapeType = 0;
        glm::vec3 pos{0.0f};
        float radius = 0.0f;
        float halfX = 0.0f;
        float halfZ = 0.0f;
        float height = 0.0f;
    };

    struct ExternNavmeshContext
    {
        NavmeshGenerationSettings genSettings{};
        std::vector<GeometryInstance> geometries;
        std::vector<OffmeshLink> offmeshLinks;
        AutoOffmeshGenerationParams autoOffmeshParams{};
        AutoOffmeshGenerationParamsV2 autoOffmeshParamsV2{};
        NavMeshData navData;
        dtNavMeshQuery* navQuery = nullptr;
        bool hasBoundingBox = false;
        glm::vec3 bboxMin{0.0f};
        glm::vec3 bboxMax{0.0f};
        bool rebuildAll = false;
        std::vector<std::pair<glm::vec3, glm::vec3>> dirtyBounds;
        float cachedExtents[3]{20.0f, 10.0f, 20.0f};
        std::string cacheRoot;
        std::string sessionId;
        int maxResidentTiles = 256;
        bool streamingEnabled = false;
        bool worldTileStreamingEnabled = false;
        bool worldUnloadBuiltTilesAfterSave = false;
        std::unordered_map<uint64_t, uint32_t> residentStamp;
        std::unordered_set<uint64_t> residentTiles;
        std::unordered_map<uint32_t, std::unordered_set<uint64_t>> agentResidentTiles;
        uint32_t stampCounter = 0;
        std::unordered_map<uint64_t, TileDbIndexEntry> dbIndexCache;
        bool dbIndexLoaded = false;
        std::filesystem::file_time_type dbMTime{};
        struct WorldGeomRecord
        {
            struct WorldGeomChunk
            {
                glm::vec3 bmin{0.0f};
                glm::vec3 bmax{0.0f};
                std::vector<uint32_t> triIndices;
            };
            struct WorldGeomSpatialCache
            {
                std::vector<WorldGeomChunk> chunks;
                uint64_t sourceHash = 0;
                float cellSize = 64.0f;
                float originX = 0.0f;
                float originZ = 0.0f;
                std::unordered_map<uint64_t, std::vector<int>> cellToChunks;
            };
            std::string id;
            std::string path;
            glm::vec3 position{0.0f};
            glm::vec3 rotation{0.0f};
            glm::vec3 worldBMin{0.0f};
            glm::vec3 worldBMax{0.0f};
            uint64_t geomHash = 0;
            uint64_t fileMTime = 0;
            uint64_t fileSize = 0;
            bool preferBin = false;
            uint32_t flags = WORLD_GEOM_PERSISTENT;
            std::string groupId = "default";
            bool loaded = false;
            bool indexed = false;
            bool spatialCacheBuilt = false;
            std::vector<uint64_t> touchedTileKeys;
            WorldGeomSpatialCache spatialCache;
            std::vector<glm::vec3> transformedVertices;
            uint64_t transformedHash = 0;
            LoadedGeometry source;
        };
        std::unordered_map<std::string, WorldGeomRecord> worldGeometry;
        std::vector<std::string> pendingWorldGeometryQueue;
        std::unordered_set<std::string> pendingWorldGeometrySet;
        std::unordered_map<uint64_t, std::vector<std::string>> tileToGeometryIds;
        std::unordered_map<std::string, std::unordered_set<uint64_t>> geomToTiles;
        std::unordered_set<uint64_t> dirtyWorldTiles;
        std::unordered_map<uint64_t, std::vector<OffmeshLink>> worldOffmeshLinksByTile;
        std::unordered_set<uint64_t> dirtyWorldOffmeshTiles;
        bool worldAutoGenerateOffmeshLinks = false;
        std::deque<uint64_t> pendingTileBuildQueue;
        std::unordered_set<uint64_t> pendingTileBuildSet;
        std::unordered_set<uint64_t> emptyWorldTiles;
        std::unordered_map<uint64_t, uint64_t> emptyWorldTileHashes;
        std::unordered_set<uint64_t> failedWorldTiles;
        bool worldManifestLoaded = false;
        bool worldAutoSaveManifest = false;
        std::unordered_map<std::uint32_t, SimAgentState> simAgents;
        std::vector<std::uint32_t> simAgentIds;
        std::unordered_map<std::uint32_t, DynObstacleState> dynObstacles;
        std::vector<std::uint32_t> dynObstacleIds;
        HeightSampler heightSampler;
        SimParamsFFI lastSimParams{};
        bool hasLastSimParams = false;
    };

    std::filesystem::path GetSessionCachePath(const ExternNavmeshContext& ctx);
    std::filesystem::path GetWorldManifestPath(const ExternNavmeshContext& ctx);
    bool EnsureNavQuery(ExternNavmeshContext& ctx);
    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild);
    bool SaveWorldTileManifestInternal(ExternNavmeshContext& ctx);
    bool LoadWorldTileManifestInternal(ExternNavmeshContext& ctx);
    bool IsWorldGeometryRecordUpToDate(const ExternNavmeshContext::WorldGeomRecord& record);
    void EnqueueTileBuild(ExternNavmeshContext& ctx, uint64_t tileKey);
    bool BuildWorldTileGeometry(ExternNavmeshContext& ctx, int tx, int ty, std::vector<glm::vec3>& outVerts, std::vector<unsigned int>& outIndices, bool* outAbortedByTriLimit);

    uint64_t WorldHashCombine64(uint64_t seed, uint64_t v)
    {
        seed ^= v + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    }

    uint64_t ComputeWorldGeometryHash(const std::string& path,
                                  const glm::vec3& pos,
                                  const glm::vec3& rot,
                                  const glm::vec3& bmin,
                                  const glm::vec3& bmax)
    {
        uint64_t h = std::hash<std::string>{}(path);
        auto hashFloat = [](float value) -> uint64_t
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return static_cast<uint64_t>(bits);
        };

        h = WorldHashCombine64(h, hashFloat(pos.x));
        h = WorldHashCombine64(h, hashFloat(pos.y));
        h = WorldHashCombine64(h, hashFloat(pos.z));
        h = WorldHashCombine64(h, hashFloat(rot.x));
        h = WorldHashCombine64(h, hashFloat(rot.y));
        h = WorldHashCombine64(h, hashFloat(rot.z));
        h = WorldHashCombine64(h, hashFloat(bmin.x));
        h = WorldHashCombine64(h, hashFloat(bmin.y));
        h = WorldHashCombine64(h, hashFloat(bmin.z));
        h = WorldHashCombine64(h, hashFloat(bmax.x));
        h = WorldHashCombine64(h, hashFloat(bmax.y));
        h = WorldHashCombine64(h, hashFloat(bmax.z));

        std::error_code ec;
        const auto mtime = std::filesystem::last_write_time(path, ec);
        if (!ec)
        {
            const auto mt = mtime.time_since_epoch().count();
            h = WorldHashCombine64(h, static_cast<uint64_t>(mt));
        }
        return h;
    }

    uint64_t GetFileMTimeHash(const std::string& path)
    {
        std::error_code ec;
        const auto mtime = std::filesystem::last_write_time(path, ec);
        if (ec)
            return 0;
        return static_cast<uint64_t>(mtime.time_since_epoch().count());
    }

    uint64_t GetFileSizeBytes(const std::string& path)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec)
            return 0;
        return static_cast<uint64_t>(size);
    }

    std::filesystem::path GetWorldManifestPath(const ExternNavmeshContext& ctx)
    {
        std::string session = ctx.sessionId.empty() ? "Navmesh_01" : ctx.sessionId;
        std::filesystem::path root = ctx.cacheRoot.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(ctx.cacheRoot);
        return root / (session + ".worldmanifest.json");
    }

    uint64_t ComputeSettingsHash(const NavmeshGenerationSettings& s)
    {
        uint64_t h = 0;
        auto mixf = [&](float v)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            h = WorldHashCombine64(h, static_cast<uint64_t>(bits));
        };
        h = WorldHashCombine64(h, static_cast<uint64_t>(s.mode));
        mixf(s.cellSize);
        mixf(s.cellHeight);
        mixf(s.agentHeight);
        mixf(s.agentRadius);
        mixf(s.agentMaxClimb);
        mixf(s.agentMaxSlope);
        mixf(s.maxEdgeLen);
        mixf(s.maxSimplificationError);
        mixf(s.minRegionSize);
        mixf(s.mergeRegionSize);
        h = WorldHashCombine64(h, static_cast<uint64_t>(s.maxVertsPerPoly));
        mixf(s.detailSampleDist);
        mixf(s.detailSampleMaxError);
        h = WorldHashCombine64(h, static_cast<uint64_t>(s.tileSize));
        h = WorldHashCombine64(h, static_cast<uint64_t>(s.maxTilesOverride));
        h = WorldHashCombine64(h, static_cast<uint64_t>(s.desiredMaxPolysPerTile));
        return h;
    }

    glm::mat3 GetRotationMatrix(const glm::vec3& eulerDegrees)
    {
        glm::vec3 normalized = glm::vec3(
            std::fmod(eulerDegrees.x, 360.0f),
            std::fmod(eulerDegrees.y, 360.0f),
            std::fmod(eulerDegrees.z, 360.0f));
        glm::mat4 rot(1.0f);
        rot = glm::rotate(rot, glm::radians(normalized.z), glm::vec3(0, 1, 0));
        rot = glm::rotate(rot, glm::radians(normalized.x), glm::vec3(1, 0, 0));
        rot = glm::rotate(rot, glm::radians(normalized.y), glm::vec3(0, 0, 1));
        return glm::mat3(rot);
    }

    static bool WriteGeometryObj(std::ofstream& out,
                                 const LoadedGeometry& source,
                                 const glm::vec3& position,
                                 const glm::vec3& rotation,
                                 const std::string& objectName,
                                 const std::string& comment,
                                 std::size_t& globalVertexOffset)
    {
        if (!source.Valid())
            return false;

        const glm::mat3 rotationMat = GetRotationMatrix(rotation);
        out << "\no " << objectName << "\n";
        if (!comment.empty())
            out << comment;

        for (const auto& v : source.vertices)
        {
            const glm::vec3 transformed = rotationMat * v + position;
            out << "v " << transformed.x << " " << transformed.y << " " << transformed.z << "\n";
        }

        for (size_t i = 0; i + 2 < source.indices.size(); i += 3)
        {
            const std::size_t i0 = globalVertexOffset + source.indices[i];
            const std::size_t i1 = globalVertexOffset + source.indices[i + 1];
            const std::size_t i2 = globalVertexOffset + source.indices[i + 2];
            out << "f " << i0 << " " << i1 << " " << i2 << "\n";
        }

        globalVertexOffset += source.vertices.size();
        return true;
    }

    void UpdateWorldBounds(GeometryInstance& instance)
    {
        glm::vec3 bmin(FLT_MAX);
        glm::vec3 bmax(-FLT_MAX);
        glm::mat3 rot = GetRotationMatrix(instance.rotation);

        for (const auto& v : instance.source.vertices)
        {
            glm::vec3 world = rot * v + instance.position;
            bmin = glm::min(bmin, world);
            bmax = glm::max(bmax, world);
        }

        instance.worldBMin = bmin;
        instance.worldBMax = bmax;
    }

    LoadedGeometry LoadBin(const std::filesystem::path& path)
    {
        LoadedGeometry geom;
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return geom;

        uint32_t version = 0;
        uint64_t vertexCount = 0;
        uint64_t indexCount = 0;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        in.read(reinterpret_cast<char*>(&geom.bmin), sizeof(geom.bmin));
        in.read(reinterpret_cast<char*>(&geom.bmax), sizeof(geom.bmax));
        if (!in.good() || version != 1)
            return LoadedGeometry{};

        geom.vertices.resize(vertexCount);
        geom.indices.resize(indexCount);
        in.read(reinterpret_cast<char*>(geom.vertices.data()), sizeof(glm::vec3) * vertexCount);
        in.read(reinterpret_cast<char*>(geom.indices.data()), sizeof(unsigned int) * indexCount);
        if (!in.good())
            return LoadedGeometry{};

        return geom;
    }

    struct TempVertex
    {
        int v = 0;
        int vt = 0;
        int vn = 0;
    };

    TempVertex ParseFaceElement(const std::string& token)
    {
        TempVertex tv;
        sscanf(token.c_str(), "%d/%d/%d", &tv.v, &tv.vt, &tv.vn);
        if (token.find("//") != std::string::npos)
        {
            sscanf(token.c_str(), "%d//%d", &tv.v, &tv.vn);
            tv.vt = 0;
        }
        return tv;
    }

    bool SaveGeometryToBin(const std::filesystem::path& path, const LoadedGeometry& geom)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
        {
            std::cout << "BIN: failed to open for write " << path << "\n";
            return false;
        }

        uint32_t version = 1;
        uint64_t vertexCount = geom.vertices.size();
        uint64_t indexCount = geom.indices.size();

        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        out.write(reinterpret_cast<const char*>(&geom.bmin), sizeof(geom.bmin));
        out.write(reinterpret_cast<const char*>(&geom.bmax), sizeof(geom.bmax));

        out.write(reinterpret_cast<const char*>(geom.vertices.data()), sizeof(glm::vec3) * vertexCount);
        out.write(reinterpret_cast<const char*>(geom.indices.data()), sizeof(unsigned int) * indexCount);

        return true;
    }

    LoadedGeometry LoadObj(const std::filesystem::path& path)
    {
        LoadedGeometry geom;
        std::ifstream file(path);
        if (!file.is_open())
            return geom;

        std::string line;
        std::vector<glm::vec3> originalVerts;
        std::vector<unsigned int> indices;

        glm::vec3 navMinB(FLT_MAX);
        glm::vec3 navMaxB(-FLT_MAX);

        while (std::getline(file, line))
        {
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v")
            {
                float x, y, z;
                ss >> x >> y >> z;
                glm::vec3 v(x, y, z);
                originalVerts.push_back(v);
                navMinB = glm::min(navMinB, v);
                navMaxB = glm::max(navMaxB, v);
            }
            else if (type == "f")
            {
                std::vector<TempVertex> faceVerts;
                std::string token;
                while (ss >> token)
                {
                    TempVertex tv = ParseFaceElement(token);
                    if (tv.v < 0)
                    {
                        tv.v = static_cast<int>(originalVerts.size()) + tv.v + 1;
                    }
                    faceVerts.push_back(tv);
                }

                if (faceVerts.size() >= 3)
                {
                    for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
                    {
                        const TempVertex& v0 = faceVerts[0];
                        const TempVertex& v1 = faceVerts[i];
                        const TempVertex& v2 = faceVerts[i + 1];
                        if (v0.v <= 0 || v1.v <= 0 || v2.v <= 0)
                            continue;
                        indices.push_back(static_cast<unsigned int>(v0.v - 1));
                        indices.push_back(static_cast<unsigned int>(v1.v - 1));
                        indices.push_back(static_cast<unsigned int>(v2.v - 1));
                    }
                }
            }
        }

        geom.vertices = std::move(originalVerts);
        geom.indices = std::move(indices);
        geom.bmin = navMinB;
        geom.bmax = navMaxB;
        if (!geom.vertices.empty() && geom.indices.empty())
        {
            std::cout << "OBJ: " << path << " carregado com vertices, mas sem indices. Verifique formato dos faces.\n";
        }
        return geom;
    }

    LoadedGeometry LoadGeometry(const char* path, bool preferBin)
    {
        if (!path)
            return {};

        std::filesystem::path p(path);
        std::filesystem::path binPath = p;
        std::filesystem::path objPath = p;
        if (p.extension() == ".bin")
        {
            objPath.replace_extension(".obj");
        }
        else
        {
            binPath.replace_extension(".bin");
        }

        if (preferBin)
        {
            if (std::filesystem::exists(binPath))
            {
                auto geom = LoadBin(binPath);
                if (geom.Valid())
                    return geom;
            }

            auto geom = LoadObj(objPath);
            if (geom.Valid())
            {
                if (!std::filesystem::exists(binPath) && SaveGeometryToBin(binPath, geom))
                {
                    std::cout << "BIN salvo em " << binPath << "\n";
                }
                return geom;
            }
            return {};
        }

        if (std::filesystem::exists(p))
        {
            if (p.extension() == ".bin")
            {
                return LoadBin(p);
            }
            return LoadObj(p);
        }

        return {};
    }

    bool CombineGeometry(const ExternNavmeshContext& ctx,
                         std::vector<glm::vec3>& outVerts,
                         std::vector<unsigned int>& outIndices)
    {
        outVerts.clear();
        outIndices.clear();

        if (ctx.geometries.empty())
            return false;

        for (const auto& inst : ctx.geometries)
        {
            if (!inst.source.Valid())
                continue;

            const unsigned int baseIndex = static_cast<unsigned int>(outVerts.size());
            glm::mat3 rot = GetRotationMatrix(inst.rotation);
            std::vector<glm::vec3> transformed;
            transformed.reserve(inst.source.vertices.size());
            for (const auto& v : inst.source.vertices)
                transformed.push_back(rot * v + inst.position);

            if (!ctx.hasBoundingBox)
            {
                outVerts.insert(outVerts.end(), transformed.begin(), transformed.end());
                for (size_t i = 0; i < inst.source.indices.size(); i += 3)
                {
                    unsigned int i0 = inst.source.indices[i + 0];
                    unsigned int i1 = inst.source.indices[i + 1];
                    unsigned int i2 = inst.source.indices[i + 2];
                    outIndices.push_back(baseIndex + i0);
                    outIndices.push_back(baseIndex + i1);
                    outIndices.push_back(baseIndex + i2);
                }
                continue;
            }

            auto triOverlapsBBox = [&](const glm::vec3& triMin, const glm::vec3& triMax)
            {
                if (triMin.x > ctx.bboxMax.x || triMax.x < ctx.bboxMin.x) return false;
                if (triMin.y > ctx.bboxMax.y || triMax.y < ctx.bboxMin.y) return false;
                if (triMin.z > ctx.bboxMax.z || triMax.z < ctx.bboxMin.z) return false;
                return true;
            };

            std::unordered_map<unsigned int, unsigned int> remap;
            remap.reserve(inst.source.vertices.size());
            auto mapVertex = [&](unsigned int localIdx) -> unsigned int
            {
                auto it = remap.find(localIdx);
                if (it != remap.end())
                    return it->second;
                unsigned int newIdx = static_cast<unsigned int>(outVerts.size());
                remap.emplace(localIdx, newIdx);
                outVerts.push_back(transformed[localIdx]);
                return newIdx;
            };

            for (size_t i = 0; i < inst.source.indices.size(); i += 3)
            {
                unsigned int i0 = inst.source.indices[i + 0];
                unsigned int i1 = inst.source.indices[i + 1];
                unsigned int i2 = inst.source.indices[i + 2];

                const glm::vec3& v0 = transformed[i0];
                const glm::vec3& v1 = transformed[i1];
                const glm::vec3& v2 = transformed[i2];
                glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
                glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);
                if (!triOverlapsBBox(triMin, triMax))
                    continue;

                unsigned int ni0 = mapVertex(i0);
                unsigned int ni1 = mapVertex(i1);
                unsigned int ni2 = mapVertex(i2);
                outIndices.push_back(ni0);
                outIndices.push_back(ni1);
                outIndices.push_back(ni2);
            }
        }

        return !outVerts.empty() && !outIndices.empty();
    }


    static constexpr uint32_t RUNTIME_CACHE_MAGIC = ('G' << 24) | ('N' << 16) | ('R' << 8) | 'C';
    static constexpr uint32_t RUNTIME_CACHE_VERSION = 1;

    struct RuntimeCacheHeader
    {
        uint32_t magic = RUNTIME_CACHE_MAGIC;
        uint32_t version = RUNTIME_CACHE_VERSION;
        uint32_t geometryCount = 0;
        uint32_t offmeshCount = 0;
        uint8_t hasBoundingBox = 0;
        uint8_t hasTileDb = 0;
        uint16_t reserved = 0;
        int32_t maxResidentTiles = 0;
    };

    template <typename T>
    bool WriteValue(std::ofstream& out, const T& value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
        return out.good();
    }

    template <typename T>
    bool ReadValue(std::ifstream& in, T& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
        return in.good();
    }

    bool WriteString(std::ofstream& out, const std::string& text)
    {
        uint32_t len = static_cast<uint32_t>(text.size());
        if (!WriteValue(out, len))
            return false;
        if (len == 0)
            return true;
        out.write(text.data(), len);
        return out.good();
    }

    bool ReadString(std::ifstream& in, std::string& text)
    {
        uint32_t len = 0;
        if (!ReadValue(in, len))
            return false;
        text.resize(len);
        if (len == 0)
            return true;
        in.read(text.data(), len);
        return in.good();
    }

    bool SaveRuntimeCacheFile(ExternNavmeshContext& ctx, const std::filesystem::path& cacheFilePath)
    {
        if (!ctx.navData.IsLoaded() || !ctx.navData.HasTiledCache())
        {
            printf("[ExternC] SaveRuntimeCacheFile: navmesh nao esta carregado em modo tiled.\n");
            return false;
        }

        if ((ctx.rebuildAll || !ctx.dirtyBounds.empty()) && !UpdateNavmeshState(ctx, false))
        {
            printf("[ExternC] SaveRuntimeCacheFile: falha ao sincronizar navmesh antes de salvar cache.\n");
            return false;
        }

        std::vector<char> tileDbBytes;
        std::filesystem::path tileDbPath = GetSessionCachePath(ctx);
        if (std::filesystem::exists(tileDbPath))
        {
            std::ifstream tileDbIn(tileDbPath, std::ios::binary);
            if (tileDbIn.is_open())
            {
                tileDbIn.seekg(0, std::ios::end);
                const auto tileDbSize = tileDbIn.tellg();
                if (tileDbSize > 0)
                {
                    tileDbIn.seekg(0, std::ios::beg);
                    tileDbBytes.resize(static_cast<size_t>(tileDbSize));
                    tileDbIn.read(tileDbBytes.data(), tileDbBytes.size());
                    if (!tileDbIn.good())
                        tileDbBytes.clear();
                }
            }
        }

        if (tileDbBytes.empty())
            printf("[ExternC] SaveRuntimeCacheFile: tile DB ausente/invalido; cache sera salvo sem snapshot de tiles (fallback por rebuild no load).\n");

        if (cacheFilePath.has_parent_path())
            std::filesystem::create_directories(cacheFilePath.parent_path());

        std::ofstream out(cacheFilePath, std::ios::binary);
        if (!out.is_open())
            return false;

        RuntimeCacheHeader header{};
        header.geometryCount = static_cast<uint32_t>(ctx.geometries.size());
        header.offmeshCount = static_cast<uint32_t>(ctx.offmeshLinks.size());
        header.hasBoundingBox = ctx.hasBoundingBox ? 1 : 0;
        header.hasTileDb = tileDbBytes.empty() ? 0 : 1;
        header.maxResidentTiles = ctx.maxResidentTiles;

        if (!WriteValue(out, header) ||
            !WriteValue(out, ctx.genSettings) ||
            !WriteValue(out, ctx.autoOffmeshParams) ||
            !WriteValue(out, ctx.bboxMin) ||
            !WriteValue(out, ctx.bboxMax) ||
            !WriteValue(out, ctx.cachedExtents))
        {
            return false;
        }

        for (const auto& geom : ctx.geometries)
        {
            const uint64_t vertexCount = static_cast<uint64_t>(geom.source.vertices.size());
            const uint64_t indexCount = static_cast<uint64_t>(geom.source.indices.size());
            if (!WriteString(out, geom.id) ||
                !WriteValue(out, geom.position) ||
                !WriteValue(out, geom.rotation) ||
                !WriteValue(out, geom.source.bmin) ||
                !WriteValue(out, geom.source.bmax) ||
                !WriteValue(out, vertexCount) ||
                !WriteValue(out, indexCount))
            {
                return false;
            }

            if (vertexCount > 0)
            {
                out.write(reinterpret_cast<const char*>(geom.source.vertices.data()), sizeof(glm::vec3) * vertexCount);
                if (!out.good())
                    return false;
            }

            if (indexCount > 0)
            {
                out.write(reinterpret_cast<const char*>(geom.source.indices.data()), sizeof(unsigned int) * indexCount);
                if (!out.good())
                    return false;
            }
        }

        for (const auto& link : ctx.offmeshLinks)
        {
            if (!WriteValue(out, link))
                return false;
        }

        const uint64_t tileDbSizeU64 = static_cast<uint64_t>(tileDbBytes.size());
        if (!WriteValue(out, tileDbSizeU64))
            return false;
        out.write(tileDbBytes.data(), tileDbBytes.size());
        if (!out.good())
            return false;

        printf("[ExternC] SaveRuntimeCacheFile: cache salvo em %s (geoms=%u links=%u tileDbBytes=%llu).\n",
               cacheFilePath.string().c_str(),
               header.geometryCount,
               header.offmeshCount,
               static_cast<unsigned long long>(tileDbSizeU64));
        return true;
    }

    bool BuildNavmeshInternal(ExternNavmeshContext& ctx)
    {
        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        if (!CombineGeometry(ctx, verts, indices))
            return false;

        ctx.rebuildAll = false;
        ctx.dirtyBounds.clear();
        ctx.navData.SetOffmeshLinks(ctx.offmeshLinks);

        const bool isTiled = ctx.genSettings.mode == NavmeshBuildMode::Tiled;
        std::filesystem::path cachePath = GetSessionCachePath(ctx);
        const float* forcedBMin = nullptr;
        const float* forcedBMax = nullptr;
        float forcedMin[3];
        float forcedMax[3];
        if (isTiled && ctx.hasBoundingBox)
        {
            forcedMin[0] = ctx.bboxMin.x;
            forcedMin[1] = ctx.bboxMin.y;
            forcedMin[2] = ctx.bboxMin.z;
            forcedMax[0] = ctx.bboxMax.x;
            forcedMax[1] = ctx.bboxMax.y;
            forcedMax[2] = ctx.bboxMax.z;
            forcedBMin = forcedMin;
            forcedBMax = forcedMax;
        }

        if (!ctx.navData.BuildFromMesh(verts, indices, ctx.genSettings, isTiled, nullptr, true, cachePath.string().c_str(), forcedBMin, forcedBMax))
            return false;

        return EnsureNavQuery(ctx);
    }

    bool LoadRuntimeCacheFile(ExternNavmeshContext& ctx, const std::filesystem::path& cacheFilePath)
    {
        std::ifstream in(cacheFilePath, std::ios::binary);
        if (!in.is_open())
        {
            printf("[ExternC] LoadRuntimeCacheFile: falha ao abrir cache %s\n", cacheFilePath.string().c_str());
            return false;
        }

        RuntimeCacheHeader header{};
        if (!ReadValue(in, header))
            return false;
        if (header.magic != RUNTIME_CACHE_MAGIC || header.version != RUNTIME_CACHE_VERSION)
        {
            printf("[ExternC] LoadRuntimeCacheFile: cabecalho invalido/incompativel em %s\n", cacheFilePath.string().c_str());
            return false;
        }

        ExternNavmeshContext loaded{};
        if (!ReadValue(in, loaded.genSettings) ||
            !ReadValue(in, loaded.autoOffmeshParams) ||
            !ReadValue(in, loaded.bboxMin) ||
            !ReadValue(in, loaded.bboxMax) ||
            !ReadValue(in, loaded.cachedExtents))
        {
            return false;
        }

        loaded.hasBoundingBox = header.hasBoundingBox != 0;
        loaded.maxResidentTiles = std::max(1, header.maxResidentTiles);
        loaded.cacheRoot = ctx.cacheRoot;
        loaded.sessionId = ctx.sessionId;
        loaded.streamingEnabled = loaded.genSettings.mode == NavmeshBuildMode::Tiled;

        loaded.geometries.reserve(header.geometryCount);
        for (uint32_t i = 0; i < header.geometryCount; ++i)
        {
            GeometryInstance geom{};
            uint64_t vertexCount = 0;
            uint64_t indexCount = 0;
            if (!ReadString(in, geom.id) ||
                !ReadValue(in, geom.position) ||
                !ReadValue(in, geom.rotation) ||
                !ReadValue(in, geom.source.bmin) ||
                !ReadValue(in, geom.source.bmax) ||
                !ReadValue(in, vertexCount) ||
                !ReadValue(in, indexCount))
            {
                return false;
            }

            geom.source.vertices.resize(static_cast<size_t>(vertexCount));
            geom.source.indices.resize(static_cast<size_t>(indexCount));
            if (vertexCount > 0)
            {
                in.read(reinterpret_cast<char*>(geom.source.vertices.data()), sizeof(glm::vec3) * vertexCount);
                if (!in.good())
                    return false;
            }
            if (indexCount > 0)
            {
                in.read(reinterpret_cast<char*>(geom.source.indices.data()), sizeof(unsigned int) * indexCount);
                if (!in.good())
                    return false;
            }
            UpdateWorldBounds(geom);
            loaded.geometries.push_back(std::move(geom));
        }

        loaded.offmeshLinks.resize(header.offmeshCount);
        for (uint32_t i = 0; i < header.offmeshCount; ++i)
        {
            if (!ReadValue(in, loaded.offmeshLinks[i]))
                return false;
        }

        uint64_t tileDbSize = 0;
        if (!ReadValue(in, tileDbSize))
            return false;
        std::vector<char> tileDbBytes(static_cast<size_t>(tileDbSize));
        if (tileDbSize > 0)
        {
            in.read(tileDbBytes.data(), static_cast<std::streamsize>(tileDbSize));
            if (!in.good())
                return false;
        }

        const float forcedMin[3] = { loaded.hasBoundingBox ? loaded.bboxMin.x : 0.0f,
                                     loaded.hasBoundingBox ? loaded.bboxMin.y : 0.0f,
                                     loaded.hasBoundingBox ? loaded.bboxMin.z : 0.0f };
        const float forcedMax[3] = { loaded.hasBoundingBox ? loaded.bboxMax.x : 0.0f,
                                     loaded.hasBoundingBox ? loaded.bboxMax.y : 0.0f,
                                     loaded.hasBoundingBox ? loaded.bboxMax.z : 0.0f };

        float runtimeMin[3] = { forcedMin[0], forcedMin[1], forcedMin[2] };
        float runtimeMax[3] = { forcedMax[0], forcedMax[1], forcedMax[2] };

        if (!loaded.hasBoundingBox)
        {
            glm::vec3 sceneMin(FLT_MAX);
            glm::vec3 sceneMax(-FLT_MAX);
            for (const auto& geom : loaded.geometries)
            {
                sceneMin = glm::min(sceneMin, geom.worldBMin);
                sceneMax = glm::max(sceneMax, geom.worldBMax);
            }
            if (sceneMin.x <= sceneMax.x)
            {
                runtimeMin[0] = sceneMin.x;
                runtimeMin[1] = sceneMin.y;
                runtimeMin[2] = sceneMin.z;
                runtimeMax[0] = sceneMax.x;
                runtimeMax[1] = sceneMax.y;
                runtimeMax[2] = sceneMax.z;
            }
        }

        bool loadedFromTileDb = false;
        if (header.hasTileDb != 0 && !tileDbBytes.empty())
        {
            loaded.navData.SetOffmeshLinks(loaded.offmeshLinks);
            if (!loaded.navData.InitTiledGrid(loaded.genSettings, runtimeMin, runtimeMax))
                return false;

            std::filesystem::path tileDbPath = GetSessionCachePath(loaded);
            if (tileDbPath.has_parent_path())
                std::filesystem::create_directories(tileDbPath.parent_path());

            {
                std::ofstream tileDbOut(tileDbPath, std::ios::binary);
                if (!tileDbOut.is_open())
                {
                    printf("[ExternC] LoadRuntimeCacheFile: falha ao escrever tile DB em %s\n", tileDbPath.string().c_str());
                    return false;
                }
                tileDbOut.write(tileDbBytes.data(), static_cast<std::streamsize>(tileDbBytes.size()));
                if (!tileDbOut.good())
                {
                    printf("[ExternC] LoadRuntimeCacheFile: escrita incompleta do tile DB em %s\n", tileDbPath.string().c_str());
                    return false;
                }
            }

            int loadedCount = 0;
            if (LoadTilesInBoundsFromDb(tileDbPath.string().c_str(), loaded.navData.GetNavMesh(), runtimeMin, runtimeMax, loadedCount))
            {
                printf("[ExternC] LoadRuntimeCacheFile: %d tiles carregados do DB.\n", loadedCount);
                loadedFromTileDb = true;
            }
            else
            {
                printf("[ExternC] LoadRuntimeCacheFile: falha ao carregar tiles do DB %s; fallback para rebuild completo.\n", tileDbPath.string().c_str());
            }
        }

        if (!loadedFromTileDb)
        {
            printf("[ExternC] LoadRuntimeCacheFile: reconstruindo navmesh a partir das geometrias/offmesh links salvos.\n");
            if (!BuildNavmeshInternal(loaded))
                return false;
        }

        loaded.dbIndexCache.clear();
        loaded.dbIndexLoaded = false;
        loaded.dbMTime = {};
        loaded.rebuildAll = false;
        loaded.dirtyBounds.clear();

        if (!EnsureNavQuery(loaded))
            return false;

        if (ctx.navQuery)
            dtFreeNavMeshQuery(ctx.navQuery);

        ctx = std::move(loaded);
        return true;
    }

    std::filesystem::path GetSessionCachePath(const ExternNavmeshContext& ctx)
    {
        const char* defaultSessionId = "Navmesh_01";
        std::string session = ctx.sessionId.empty() ? defaultSessionId : ctx.sessionId;
        std::filesystem::path root = ctx.cacheRoot.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(ctx.cacheRoot);
        return root / (session + ".tilecache");
    }

    bool SaveWorldTileManifestInternal(ExternNavmeshContext& ctx)
    {
        if (!ctx.worldTileStreamingEnabled)
            return false;

        nlohmann::json j;
        j["version"] = 2;
        j["cacheRoot"] = ctx.cacheRoot;
        j["sessionId"] = ctx.sessionId;
        j["bboxMin"] = { ctx.bboxMin.x, ctx.bboxMin.y, ctx.bboxMin.z };
        j["bboxMax"] = { ctx.bboxMax.x, ctx.bboxMax.y, ctx.bboxMax.z };
        j["settingsHash"] = ComputeSettingsHash(ctx.genSettings);
        j["settings"] = {
            {"mode", static_cast<int>(ctx.genSettings.mode)},
            {"cellSize", ctx.genSettings.cellSize},
            {"cellHeight", ctx.genSettings.cellHeight},
            {"agentHeight", ctx.genSettings.agentHeight},
            {"agentRadius", ctx.genSettings.agentRadius},
            {"agentMaxClimb", ctx.genSettings.agentMaxClimb},
            {"agentMaxSlope", ctx.genSettings.agentMaxSlope},
            {"maxEdgeLen", ctx.genSettings.maxEdgeLen},
            {"maxSimplificationError", ctx.genSettings.maxSimplificationError},
            {"minRegionSize", ctx.genSettings.minRegionSize},
            {"mergeRegionSize", ctx.genSettings.mergeRegionSize},
            {"maxVertsPerPoly", ctx.genSettings.maxVertsPerPoly},
            {"detailSampleDist", ctx.genSettings.detailSampleDist},
            {"detailSampleMaxError", ctx.genSettings.detailSampleMaxError},
            {"tileSize", ctx.genSettings.tileSize},
            {"maxTilesOverride", ctx.genSettings.maxTilesOverride},
            {"desiredMaxPolysPerTile", ctx.genSettings.desiredMaxPolysPerTile}
        };

        auto isPersistentManifestGeom = [&](const std::string& id) -> bool {
            auto it = ctx.worldGeometry.find(id);
            if (it == ctx.worldGeometry.end())
                return false;
            const auto& r = it->second;
            return (r.flags & WORLD_GEOM_PERSISTENT) != 0 &&
                   (r.flags & WORLD_GEOM_DYNAMIC) == 0;
        };

        std::unordered_set<uint64_t> persistentTileKeys;
        size_t persistentGeoms = 0;

        j["geometries"] = nlohmann::json::array();
        for (const auto& kv : ctx.worldGeometry)
        {
            const auto& rec = kv.second;
            if ((rec.flags & WORLD_GEOM_PERSISTENT) == 0 || (rec.flags & WORLD_GEOM_DYNAMIC) != 0)
                continue;
            ++persistentGeoms;
            nlohmann::json g;
            g["customID"] = rec.id;
            g["path"] = rec.path;
            g["preferBIN"] = rec.preferBin;
            g["position"] = { rec.position.x, rec.position.y, rec.position.z };
            g["rotation"] = { rec.rotation.x, rec.rotation.y, rec.rotation.z };
            g["worldBMin"] = { rec.worldBMin.x, rec.worldBMin.y, rec.worldBMin.z };
            g["worldBMax"] = { rec.worldBMax.x, rec.worldBMax.y, rec.worldBMax.z };
            g["geomHash"] = rec.geomHash;
            g["fileMTime"] = rec.fileMTime;
            g["fileSize"] = rec.fileSize;
            g["flags"] = rec.flags;
            g["groupId"] = rec.groupId;
            g["loaded"] = rec.loaded;
            g["indexed"] = rec.indexed;
            g["tileKeys"] = rec.touchedTileKeys;
            j["geometries"].push_back(std::move(g));
            for (uint64_t k : rec.touchedTileKeys)
                persistentTileKeys.insert(k);
        }

        std::vector<std::string> pendingPersistentGeoms;
        for (const auto& id : ctx.pendingWorldGeometryQueue)
        {
            if (isPersistentManifestGeom(id))
                pendingPersistentGeoms.push_back(id);
        }
        j["pendingWorldGeometryQueue"] = std::move(pendingPersistentGeoms);

        std::vector<uint64_t> dirtyPersistentTiles;
        for (uint64_t key : ctx.dirtyWorldTiles)
        {
            if (persistentTileKeys.find(key) != persistentTileKeys.end())
                dirtyPersistentTiles.push_back(key);
        }
        j["dirtyWorldTiles"] = std::move(dirtyPersistentTiles);

        std::vector<uint64_t> pendingPersistentTiles;
        for (uint64_t key : ctx.pendingTileBuildQueue)
        {
            if (persistentTileKeys.find(key) != persistentTileKeys.end())
                pendingPersistentTiles.push_back(key);
        }
        j["pendingTileBuildQueue"] = std::move(pendingPersistentTiles);

        std::vector<uint64_t> emptyPersistentTiles;
        for (uint64_t key : ctx.emptyWorldTiles)
        {
            if (persistentTileKeys.find(key) != persistentTileKeys.end())
                emptyPersistentTiles.push_back(key);
        }
        j["emptyWorldTiles"] = std::move(emptyPersistentTiles);

        std::vector<uint64_t> failedPersistentTiles;
        for (uint64_t key : ctx.failedWorldTiles)
        {
            if (persistentTileKeys.find(key) != persistentTileKeys.end())
                failedPersistentTiles.push_back(key);
        }
        j["failedWorldTiles"] = std::move(failedPersistentTiles);
        nlohmann::json emptyHash = nlohmann::json::object();
        for (const auto& kv : ctx.emptyWorldTileHashes)
        {
            if (persistentTileKeys.find(kv.first) != persistentTileKeys.end())
                emptyHash[std::to_string(kv.first)] = kv.second;
        }
        j["emptyWorldTileHashes"] = std::move(emptyHash);
        nlohmann::json tileToGeom = nlohmann::json::object();
        for (const auto& kv : ctx.tileToGeometryIds)
        {
            std::vector<std::string> filtered;
            for (const auto& id : kv.second)
            {
                if (isPersistentManifestGeom(id))
                    filtered.push_back(id);
            }
            if (!filtered.empty())
                tileToGeom[std::to_string(kv.first)] = std::move(filtered);
        }
        j["tileToGeometryIds"] = std::move(tileToGeom);

        const std::filesystem::path manifestPath = GetWorldManifestPath(ctx);
        if (manifestPath.has_parent_path())
            std::filesystem::create_directories(manifestPath.parent_path());
        std::ofstream out(manifestPath);
        if (!out.is_open())
            return false;
        out << j.dump(2);
        if (!out.good())
            return false;

        printf("[WorldTile] Save manifest: persistentGeoms=%zu totalGeoms=%zu dirtySaved=%zu pendingSaved=%zu tileToGeomSaved=%zu path=%s\n",
               persistentGeoms, ctx.worldGeometry.size(), j["dirtyWorldTiles"].size(), j["pendingTileBuildQueue"].size(), j["tileToGeometryIds"].size(), manifestPath.string().c_str());
        return true;
    }

    bool EnsureDbIndexLoaded(ExternNavmeshContext& ctx, const std::filesystem::path& cachePath)
    {
        if (!ctx.navData.GetNavMesh())
            return false;

        if (!std::filesystem::exists(cachePath))
        {
            ctx.dbIndexCache.clear();
            ctx.dbIndexLoaded = false;
            ctx.dbMTime = {};
            return false;
        }

        const auto mtime = std::filesystem::last_write_time(cachePath);
        if (!ctx.dbIndexLoaded || ctx.dbMTime != mtime)
        {
            ctx.dbIndexCache.clear();
            if (!TileDbLoadIndex(cachePath.string().c_str(), ctx.navData.GetNavMesh(), ctx.dbIndexCache))
            {
                ctx.dbIndexLoaded = false;
                ctx.dbMTime = mtime;
                return false;
            }
            ctx.dbIndexLoaded = true;
            ctx.dbMTime = mtime;
        }

        return ctx.dbIndexLoaded;
    }

    uint64_t ComputeWorldTileHash(ExternNavmeshContext& ctx, int tx, int ty)
    {
        uint64_t h = ComputeSettingsHash(ctx.genSettings);
        h = WorldHashCombine64(h, static_cast<uint64_t>(static_cast<uint32_t>(tx)));
        h = WorldHashCombine64(h, static_cast<uint64_t>(static_cast<uint32_t>(ty)));

        const uint64_t key = MakeTileKey(tx, ty);
        auto it = ctx.tileToGeometryIds.find(key);
        if (it != ctx.tileToGeometryIds.end())
        {
            std::vector<std::string> ids = it->second;
            std::sort(ids.begin(), ids.end());
            for (const std::string& id : ids)
            {
                auto gIt = ctx.worldGeometry.find(id);
                if (gIt == ctx.worldGeometry.end())
                    continue;
                const auto& rec = gIt->second;
                h = WorldHashCombine64(h, rec.geomHash);
                h = WorldHashCombine64(h, std::hash<std::string>{}(rec.id));
            }
        }

        for (const auto& link : ctx.offmeshLinks)
        {
            const float minX = std::min(link.start.x, link.end.x);
            const float maxX = std::max(link.start.x, link.end.x);
            const float minZ = std::min(link.start.z, link.end.z);
            const float maxZ = std::max(link.start.z, link.end.z);
            const dtNavMeshParams* params = ctx.navData.GetNavMesh() ? ctx.navData.GetNavMesh()->getParams() : nullptr;
            if (!params)
                continue;
            const float tileMinX = params->orig[0] + tx * params->tileWidth;
            const float tileMaxX = tileMinX + params->tileWidth;
            const float tileMinZ = params->orig[2] + ty * params->tileHeight;
            const float tileMaxZ = tileMinZ + params->tileHeight;
            if (minX > tileMaxX || maxX < tileMinX || minZ > tileMaxZ || maxZ < tileMinZ)
                continue;
            h = WorldHashCombine64(h, static_cast<uint64_t>(link.userId));
        }

        return h;
    }

    bool LoadWorldTileManifestInternal(ExternNavmeshContext& ctx)
    {
        if (!ctx.worldTileStreamingEnabled)
            return false;
        const std::filesystem::path manifestPath = GetWorldManifestPath(ctx);
        if (!std::filesystem::exists(manifestPath))
            return false;

        nlohmann::json j;
        try
        {
            std::ifstream in(manifestPath);
            if (!in.is_open())
                return false;
            in >> j;
        }
        catch (...)
        {
            printf("[WorldTile] Load manifest: arquivo invalido %s\n", manifestPath.string().c_str());
            return false;
        }

        const uint64_t settingsHashSaved = j.value("settingsHash", 0ull);
        const uint64_t settingsHashCurrent = ComputeSettingsHash(ctx.genSettings);
        const auto bboxMinJson = j.value("bboxMin", nlohmann::json::array());
        const auto bboxMaxJson = j.value("bboxMax", nlohmann::json::array());
        if (settingsHashSaved != settingsHashCurrent ||
            bboxMinJson.size() != 3 || bboxMaxJson.size() != 3)
        {
            printf("[WorldTile] Load manifest: incompativel (settings/bounds). iniciando nova sessao.\n");
            return false;
        }

        const glm::vec3 savedMin(bboxMinJson[0].get<float>(), bboxMinJson[1].get<float>(), bboxMinJson[2].get<float>());
        const glm::vec3 savedMax(bboxMaxJson[0].get<float>(), bboxMaxJson[1].get<float>(), bboxMaxJson[2].get<float>());
        if (glm::distance(savedMin, ctx.bboxMin) > 0.01f || glm::distance(savedMax, ctx.bboxMax) > 0.01f)
        {
            printf("[WorldTile] Load manifest: bounds diferentes, ignorando manifesto.\n");
            return false;
        }

        ctx.worldGeometry.clear();
        ctx.tileToGeometryIds.clear();
        ctx.geomToTiles.clear();
        ctx.dirtyWorldTiles.clear();
        ctx.pendingTileBuildQueue.clear();
        ctx.pendingTileBuildSet.clear();
        ctx.pendingWorldGeometryQueue.clear();
        ctx.pendingWorldGeometrySet.clear();
        ctx.emptyWorldTiles.clear();
        ctx.emptyWorldTileHashes.clear();
        ctx.failedWorldTiles.clear();

        int changed = 0;
        int removed = 0;
        int loadedCount = 0;
        const auto geoms = j.value("geometries", nlohmann::json::array());
        const bool hasTileToGeometryIds = j.contains("tileToGeometryIds") && j["tileToGeometryIds"].is_object();
        int indexedGeometries = 0;
        for (const auto& g : geoms)
        {
            ExternNavmeshContext::WorldGeomRecord rec{};
            rec.id = g.value("customID", "");
            rec.path = g.value("path", "");
            rec.preferBin = g.value("preferBIN", false);
            rec.flags = g.value("flags", static_cast<uint32_t>(WORLD_GEOM_PERSISTENT));
            rec.groupId = g.value("groupId", std::string("default"));
            const auto p = g.value("position", nlohmann::json::array());
            const auto r = g.value("rotation", nlohmann::json::array());
            const auto bmin = g.value("worldBMin", nlohmann::json::array());
            const auto bmax = g.value("worldBMax", nlohmann::json::array());
            if (rec.id.empty() || p.size() != 3 || r.size() != 3 || bmin.size() != 3 || bmax.size() != 3)
                continue;
            if ((rec.flags & WORLD_GEOM_DYNAMIC) != 0)
                continue;
            if ((rec.flags & WORLD_GEOM_PERSISTENT) == 0)
                continue;
            rec.position = glm::vec3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
            rec.rotation = glm::vec3(r[0].get<float>(), r[1].get<float>(), r[2].get<float>());
            rec.worldBMin = glm::vec3(bmin[0].get<float>(), bmin[1].get<float>(), bmin[2].get<float>());
            rec.worldBMax = glm::vec3(bmax[0].get<float>(), bmax[1].get<float>(), bmax[2].get<float>());
            rec.geomHash = g.value("geomHash", 0ull);
            rec.fileMTime = g.value("fileMTime", 0ull);
            rec.fileSize = g.value("fileSize", 0ull);
            rec.loaded = false;
            rec.indexed = g.value("indexed", false);
            rec.touchedTileKeys = g.value("tileKeys", std::vector<uint64_t>{});

            if (!std::filesystem::exists(rec.path))
            {
                ++removed;
                for (uint64_t k : rec.touchedTileKeys)
                {
                    ctx.dirtyWorldTiles.insert(k);
                    ctx.dirtyWorldOffmeshTiles.insert(k);
                }
                continue;
            }

            if (!IsWorldGeometryRecordUpToDate(rec))
            {
                ++changed;
                for (uint64_t k : rec.touchedTileKeys)
                {
                    ctx.dirtyWorldTiles.insert(k);
                    ctx.dirtyWorldOffmeshTiles.insert(k);
                }
                if (ctx.pendingWorldGeometrySet.insert(rec.id).second)
                    ctx.pendingWorldGeometryQueue.push_back(rec.id);
            }
            else
            {
                ++loadedCount;
                if (!hasTileToGeometryIds)
                {
                    for (uint64_t k : rec.touchedTileKeys)
                    {
                        auto& vec = ctx.tileToGeometryIds[k];
                        if (std::find(vec.begin(), vec.end(), rec.id) == vec.end())
                            vec.push_back(rec.id);
                        ctx.geomToTiles[rec.id].insert(k);
                    }
                    if (rec.indexed)
                        ++indexedGeometries;
                }
            }

            ctx.worldGeometry[rec.id] = std::move(rec);
        }


        if (hasTileToGeometryIds)
        {
            const auto& tileToGeomJson = j["tileToGeometryIds"];
            for (auto it = tileToGeomJson.begin(); it != tileToGeomJson.end(); ++it)
            {
                const uint64_t tileKey = std::stoull(it.key());
                if (!it.value().is_array())
                    continue;
                auto& vec = ctx.tileToGeometryIds[tileKey];
                for (const auto& idJson : it.value())
                {
                    if (!idJson.is_string())
                        continue;
                    const std::string geomId = idJson.get<std::string>();
                    if (ctx.worldGeometry.find(geomId) == ctx.worldGeometry.end())
                        continue;
                    if (std::find(vec.begin(), vec.end(), geomId) == vec.end())
                        vec.push_back(geomId);
                    ctx.geomToTiles[geomId].insert(tileKey);
                }
            }

            for (auto& kv : ctx.worldGeometry)
            {
                auto itGeomTiles = ctx.geomToTiles.find(kv.first);
                kv.second.touchedTileKeys.clear();
                if (itGeomTiles != ctx.geomToTiles.end())
                {
                    kv.second.touchedTileKeys.assign(itGeomTiles->second.begin(), itGeomTiles->second.end());
                    if (!kv.second.touchedTileKeys.empty())
                    {
                        kv.second.indexed = true;
                        ++indexedGeometries;
                    }
                }
            }
        }
        const auto pendingGeom = j.value("pendingWorldGeometryQueue", std::vector<std::string>{});
        for (const auto& id : pendingGeom)
        {
            if (ctx.worldGeometry.find(id) == ctx.worldGeometry.end())
                continue;
            if (ctx.pendingWorldGeometrySet.insert(id).second)
                ctx.pendingWorldGeometryQueue.push_back(id);
        }

        const auto dirtyTiles = j.value("dirtyWorldTiles", std::vector<uint64_t>{});
        for (uint64_t key : dirtyTiles)
            ctx.dirtyWorldTiles.insert(key);

        const auto pendingTiles = j.value("pendingTileBuildQueue", std::vector<uint64_t>{});
        for (uint64_t key : pendingTiles)
            EnqueueTileBuild(ctx, key);

        std::filesystem::path cachePath = GetSessionCachePath(ctx);
        const bool indexLoaded = EnsureDbIndexLoaded(ctx, cachePath);
        const size_t dbTiles = indexLoaded ? ctx.dbIndexCache.size() : 0;
        const size_t pendingBefore = ctx.pendingTileBuildQueue.size();
        const size_t dirtyBefore = ctx.dirtyWorldTiles.size();
        size_t skippedCached = 0;
        if (indexLoaded)
        {
            auto removeIfCached = [&](uint64_t key)
            {
                const int tx = static_cast<int>(key >> 32);
                const int ty = static_cast<int>(key & 0xffffffffu);
                const uint64_t expectedHash = ComputeWorldTileHash(ctx, tx, ty);
                auto itDb = ctx.dbIndexCache.find(key);
                if (itDb != ctx.dbIndexCache.end() && itDb->second.geomHash == expectedHash)
                {
                    ctx.pendingTileBuildSet.erase(key);
                    ctx.dirtyWorldTiles.erase(key);
                    ++skippedCached;
                    return true;
                }
                return false;
            };

            std::deque<uint64_t> filtered;
            for (uint64_t key : ctx.pendingTileBuildQueue)
            {
                if (!removeIfCached(key))
                    filtered.push_back(key);
            }
            ctx.pendingTileBuildQueue.swap(filtered);

            std::vector<uint64_t> dirtySnapshot(ctx.dirtyWorldTiles.begin(), ctx.dirtyWorldTiles.end());
            for (uint64_t key : dirtySnapshot)
                removeIfCached(key);
        }
        const auto emptyTiles = j.value("emptyWorldTiles", std::vector<uint64_t>{});
        for (uint64_t key : emptyTiles)
            ctx.emptyWorldTiles.insert(key);
        const auto failedTiles = j.value("failedWorldTiles", std::vector<uint64_t>{});
        for (uint64_t key : failedTiles)
            ctx.failedWorldTiles.insert(key);
        const auto emptyHashesJson = j.value("emptyWorldTileHashes", nlohmann::json::object());
        if (emptyHashesJson.is_object())
        {
            for (auto it = emptyHashesJson.begin(); it != emptyHashesJson.end(); ++it)
                ctx.emptyWorldTileHashes[std::stoull(it.key())] = it.value().get<uint64_t>();
        }
        if (ctx.pendingTileBuildQueue.empty() && !ctx.dirtyWorldTiles.empty())
        {
            std::vector<uint64_t> dirtySnapshot(ctx.dirtyWorldTiles.begin(), ctx.dirtyWorldTiles.end());
            for (uint64_t key : dirtySnapshot)
                EnqueueTileBuild(ctx, key);
        }

        ctx.worldManifestLoaded = true;
        printf("[WorldTile] Resume: dbTiles=%zu pendingBefore=%zu dirtyBefore=%zu skippedCached=%zu pendingAfter=%zu dirtyAfter=%zu\n",
               dbTiles, pendingBefore, dirtyBefore, skippedCached, ctx.pendingTileBuildQueue.size(), ctx.dirtyWorldTiles.size());
        printf("[WorldTile] Load manifest: loaded=%d changed=%d removed=%d pendingGeoms=%zu indexedGeometries=%d dirtyTiles=%zu pendingTiles=%zu\n",
               loadedCount, changed, removed, ctx.pendingWorldGeometryQueue.size(), indexedGeometries, ctx.dirtyWorldTiles.size(), ctx.pendingTileBuildQueue.size());
        return true;
    }

    
    bool EnsureNavQuery(ExternNavmeshContext& ctx)
    {
        if (!ctx.navData.GetNavMesh())
            return false;

        if (!ctx.navQuery)
        {
            ctx.navQuery = dtAllocNavMeshQuery();
            if (!ctx.navQuery)
                return false;
        }

        if (dtStatusFailed(ctx.navQuery->init(ctx.navData.GetNavMesh(), 2048)))
            return false;

        const float extents[3] = {
            20.0f,
            10.5f,
            20.0f
        };
        memcpy(ctx.cachedExtents, extents, sizeof(extents));
        return true;
    }

    GeometryInstance* FindGeometry(ExternNavmeshContext& ctx, const char* id)
    {
        if (!id) return nullptr;
        for (auto& g : ctx.geometries)
        {
            if (g.id == id)
                return &g;
        }
        return nullptr;
    }

    void RegisterDirtyBounds(ExternNavmeshContext& ctx, const GeometryInstance& inst)
    {
        ctx.dirtyBounds.emplace_back(inst.worldBMin, inst.worldBMax);
    }

    void EnqueueTileBuild(ExternNavmeshContext& ctx, uint64_t tileKey)
    {
        if (ctx.pendingTileBuildSet.insert(tileKey).second)
            ctx.pendingTileBuildQueue.push_back(tileKey);
    }

    void MarkTilesDirty(ExternNavmeshContext& ctx, const std::unordered_set<uint64_t>& tiles)
    {
        for (uint64_t key : tiles)
        {
            ctx.dirtyWorldTiles.insert(key);
            ctx.dirtyWorldOffmeshTiles.insert(key);
            EnqueueTileBuild(ctx, key);
        }
    }

    static uint64_t QuantHashOffmeshLink(const OffmeshLink& link, float q)
    {
        const float quant = std::max(0.001f, q);
        auto qi = [&](float v) -> int { return static_cast<int>(std::lround(v / quant)); };
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
        mix(static_cast<uint64_t>(qi(link.start.x)));
        mix(static_cast<uint64_t>(qi(link.start.y)));
        mix(static_cast<uint64_t>(qi(link.start.z)));
        mix(static_cast<uint64_t>(qi(link.end.x)));
        mix(static_cast<uint64_t>(qi(link.end.y)));
        mix(static_cast<uint64_t>(qi(link.end.z)));
        mix(static_cast<uint64_t>(link.area));
        return h;
    }

    static bool GenerateWorldOffmeshLinksForTile(ExternNavmeshContext& ctx, int tx, int ty, const AutoOffmeshGenerationParamsV2& params, std::vector<OffmeshLink>& outLinks)
    {
        outLinks.clear();
        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        if (!BuildWorldTileGeometry(ctx, tx, ty, verts, indices, nullptr))
            return true;
        std::vector<OffmeshLink> generated;
        if (!ctx.navData.GenerateAutomaticOffmeshLinksForTileV2(tx, ty, params, verts, indices, generated))
            return false;
        std::unordered_set<uint64_t> dedupe;
        const size_t rawCount = generated.size();
        for (const auto& link : generated)
        {
            const uint64_t h = QuantHashOffmeshLink(link, params.quantizePos);
            if (!dedupe.insert(h).second)
                continue;
            outLinks.push_back(link);
            if (params.maxLinksPerTile > 0 && static_cast<int>(outLinks.size()) >= params.maxLinksPerTile)
                break;
        }
        printf("[WorldOffmesh] tile %d,%d generatedRaw=%zu accepted=%zu passedToTile=%zu\n",
               tx, ty, rawCount, dedupe.size(), outLinks.size());
        return true;
    }

    void RemoveGeometryFromWorldIndex(ExternNavmeshContext& ctx, const std::string& geomId)
    {
        auto itTiles = ctx.geomToTiles.find(geomId);
        if (itTiles != ctx.geomToTiles.end())
        {
            for (uint64_t tileKey : itTiles->second)
            {
                auto itVec = ctx.tileToGeometryIds.find(tileKey);
                if (itVec == ctx.tileToGeometryIds.end())
                    continue;

                auto& ids = itVec->second;
                ids.erase(std::remove(ids.begin(), ids.end(), geomId), ids.end());
                if (ids.empty())
                    ctx.tileToGeometryIds.erase(itVec);
            }
            ctx.geomToTiles.erase(itTiles);
        }

        for (auto itVec = ctx.tileToGeometryIds.begin(); itVec != ctx.tileToGeometryIds.end();)
        {
            auto& ids = itVec->second;
            ids.erase(std::remove(ids.begin(), ids.end(), geomId), ids.end());
            if (ids.empty())
                itVec = ctx.tileToGeometryIds.erase(itVec);
            else
                ++itVec;
        }
    }

    bool IsWorldGeometryRecordUpToDate(const ExternNavmeshContext::WorldGeomRecord& record)
    {
        if (record.path.empty() || !std::filesystem::exists(record.path))
            return false;
        const uint64_t mtime = GetFileMTimeHash(record.path);
        const uint64_t fsize = GetFileSizeBytes(record.path);
        return (mtime != 0 && fsize != 0 && mtime == record.fileMTime && fsize == record.fileSize);
    }

    void EnsureTransformedVertices(ExternNavmeshContext::WorldGeomRecord& record)
    {
        if (record.transformedHash == record.geomHash && record.transformedVertices.size() == record.source.vertices.size())
            return;

        glm::mat3 rot = GetRotationMatrix(record.rotation);
        record.transformedVertices.clear();
        record.transformedVertices.reserve(record.source.vertices.size());
        for (const auto& v : record.source.vertices)
            record.transformedVertices.push_back(rot * v + record.position);
        record.transformedHash = record.geomHash;
    }

    void BuildSpatialCacheForGeometry(ExternNavmeshContext::WorldGeomRecord& record, int targetTrisPerChunk)
    {
        record.spatialCache.chunks.clear();
        record.spatialCache.cellToChunks.clear();
        record.spatialCache.sourceHash = record.geomHash;
        record.spatialCacheBuilt = false;
        if (!record.source.Valid())
            return;

        const int triCount = static_cast<int>(record.source.indices.size() / 3);
        if (triCount <= 0)
            return;

        EnsureTransformedVertices(record);
        const float triPerChunkHint = static_cast<float>(std::max(32, targetTrisPerChunk));
        const float triDensity = std::sqrt(triPerChunkHint);
        const float triWorldX = std::max(1.0f, record.worldBMax.x - record.worldBMin.x);
        const float triWorldZ = std::max(1.0f, record.worldBMax.z - record.worldBMin.z);
        const float tileWorld = std::max(16.0f, std::max(triWorldX, triWorldZ) / std::max(1.0f, triDensity));
        record.spatialCache.cellSize = std::max(64.0f, tileWorld);
        record.spatialCache.originX = record.worldBMin.x;
        record.spatialCache.originZ = record.worldBMin.z;

        auto cellCoordX = [&](float x) -> int
        {
            return static_cast<int>(std::floor((x - record.spatialCache.originX) / record.spatialCache.cellSize));
        };
        auto cellCoordZ = [&](float z) -> int
        {
            return static_cast<int>(std::floor((z - record.spatialCache.originZ) / record.spatialCache.cellSize));
        };

        for (int tri = 0; tri < triCount; ++tri)
        {
            const unsigned int i0 = record.source.indices[tri * 3 + 0];
            const unsigned int i1 = record.source.indices[tri * 3 + 1];
            const unsigned int i2 = record.source.indices[tri * 3 + 2];
            const glm::vec3& v0 = record.transformedVertices[i0];
            const glm::vec3& v1 = record.transformedVertices[i1];
            const glm::vec3& v2 = record.transformedVertices[i2];
            const glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
            const glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);

            const int minCx = cellCoordX(triMin.x);
            const int maxCx = cellCoordX(triMax.x);
            const int minCz = cellCoordZ(triMin.z);
            const int maxCz = cellCoordZ(triMax.z);
            for (int cz = minCz; cz <= maxCz; ++cz)
            {
                for (int cx = minCx; cx <= maxCx; ++cx)
                {
                    const uint64_t cellKey = MakeGridCellKey(cx, cz);
                    ExternNavmeshContext::WorldGeomRecord::WorldGeomChunk chunk{};
                    chunk.bmin = triMin;
                    chunk.bmax = triMax;
                    chunk.triIndices.push_back(static_cast<uint32_t>(tri));
                    const int chunkIndex = static_cast<int>(record.spatialCache.chunks.size());
                    record.spatialCache.chunks.push_back(std::move(chunk));
                    record.spatialCache.cellToChunks[cellKey].push_back(chunkIndex);
                }
            }
        }

        record.spatialCacheBuilt = !record.spatialCache.chunks.empty() && !record.spatialCache.cellToChunks.empty();
    }

    size_t AppendGeometryForTile(ExternNavmeshContext::WorldGeomRecord& rec,
                                 const glm::vec3& tileMin,
                                 const glm::vec3& tileMax,
                                 std::vector<glm::vec3>& outVerts,
                                 std::vector<unsigned int>& outIndices)
    {
        if (!rec.loaded || !rec.source.Valid())
            return 0;

        if (!rec.spatialCacheBuilt || rec.spatialCache.sourceHash != rec.geomHash)
            BuildSpatialCacheForGeometry(rec, 256);

        EnsureTransformedVertices(rec);
        const size_t beforeIndices = outIndices.size();

        std::unordered_map<unsigned int, unsigned int> remap;
        remap.reserve(rec.source.vertices.size() / 4 + 1);
        auto mapVertex = [&](unsigned int localIdx) -> unsigned int
        {
            auto it = remap.find(localIdx);
            if (it != remap.end())
                return it->second;
            unsigned int newIdx = static_cast<unsigned int>(outVerts.size());
            remap.emplace(localIdx, newIdx);
            outVerts.push_back(rec.transformedVertices[localIdx]);
            return newIdx;
        };

        auto triOverlaps = [&](const glm::vec3& triMin, const glm::vec3& triMax) -> bool
        {
            if (triMin.x > tileMax.x || triMax.x < tileMin.x) return false;
            if (triMin.y > tileMax.y || triMax.y < tileMin.y) return false;
            if (triMin.z > tileMax.z || triMax.z < tileMin.z) return false;
            return true;
        };

        const auto appendTri = [&](uint32_t triIdx, std::vector<unsigned int>& triOut)
        {
            const unsigned int i0 = rec.source.indices[triIdx * 3 + 0];
            const unsigned int i1 = rec.source.indices[triIdx * 3 + 1];
            const unsigned int i2 = rec.source.indices[triIdx * 3 + 2];
            const glm::vec3& v0 = rec.transformedVertices[i0];
            const glm::vec3& v1 = rec.transformedVertices[i1];
            const glm::vec3& v2 = rec.transformedVertices[i2];
            const glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
            const glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);
            if (!triOverlaps(triMin, triMax))
                return;
            triOut.push_back(mapVertex(i0));
            triOut.push_back(mapVertex(i1));
            triOut.push_back(mapVertex(i2));
        };

        if (rec.spatialCacheBuilt)
        {
            const int minCx = static_cast<int>(std::floor((tileMin.x - rec.spatialCache.originX) / rec.spatialCache.cellSize));
            const int maxCx = static_cast<int>(std::floor((tileMax.x - rec.spatialCache.originX) / rec.spatialCache.cellSize));
            const int minCz = static_cast<int>(std::floor((tileMin.z - rec.spatialCache.originZ) / rec.spatialCache.cellSize));
            const int maxCz = static_cast<int>(std::floor((tileMax.z - rec.spatialCache.originZ) / rec.spatialCache.cellSize));
            std::unordered_set<uint32_t> usedTriIndices;
            uint64_t debugCellsVisited = 0;
            uint64_t debugChunksVisited = 0;
            uint64_t debugTrisTested = 0;
            uint64_t debugTrisAccepted = 0;
            float acceptedMinY = FLT_MAX;
            float acceptedMaxY = -FLT_MAX;
            for (int cz = minCz; cz <= maxCz; ++cz)
            {
                for (int cx = minCx; cx <= maxCx; ++cx)
                {
                    ++debugCellsVisited;
                    const uint64_t cellKey = MakeGridCellKey(cx, cz);
                    auto itChunkIdxs = rec.spatialCache.cellToChunks.find(cellKey);
                    if (itChunkIdxs == rec.spatialCache.cellToChunks.end())
                        continue;
                    for (int chunkIdx : itChunkIdxs->second)
                    {
                        if (chunkIdx < 0 || static_cast<size_t>(chunkIdx) >= rec.spatialCache.chunks.size())
                            continue;
                        ++debugChunksVisited;
                        const auto& chunk = rec.spatialCache.chunks[static_cast<size_t>(chunkIdx)];
                        if (!triOverlaps(chunk.bmin, chunk.bmax))
                            continue;
                        for (uint32_t triIdx : chunk.triIndices)
                        {
                            if (!usedTriIndices.insert(triIdx).second)
                                continue;
                            ++debugTrisTested;
                            const size_t prevCount = outIndices.size();
                            appendTri(triIdx, outIndices);
                            if (outIndices.size() > prevCount)
                            {
                                ++debugTrisAccepted;
                                const unsigned int i0 = rec.source.indices[triIdx * 3 + 0];
                                const unsigned int i1 = rec.source.indices[triIdx * 3 + 1];
                                const unsigned int i2 = rec.source.indices[triIdx * 3 + 2];
                                const glm::vec3& v0 = rec.transformedVertices[i0];
                                const glm::vec3& v1 = rec.transformedVertices[i1];
                                const glm::vec3& v2 = rec.transformedVertices[i2];
                                const glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
                                const glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);
                                acceptedMinY = std::min(acceptedMinY, triMin.y);
                                acceptedMaxY = std::max(acceptedMaxY, triMax.y);
                            }
                        }
                    }
                }
            }
            printf("[WorldTile][AppendGeometryForTile] geom=%s cells=%llu chunks=%llu tested=%llu accepted=%llu acceptedY=[%.3f, %.3f] tileY=[%.3f, %.3f]\n",
                   rec.id.c_str(),
                   static_cast<unsigned long long>(debugCellsVisited),
                   static_cast<unsigned long long>(debugChunksVisited),
                   static_cast<unsigned long long>(debugTrisTested),
                   static_cast<unsigned long long>(debugTrisAccepted),
                   (debugTrisAccepted > 0 ? acceptedMinY : 0.0f),
                   (debugTrisAccepted > 0 ? acceptedMaxY : 0.0f),
                   tileMin.y, tileMax.y);
            return (outIndices.size() - beforeIndices) / 3;
        }

        const uint32_t triCount = static_cast<uint32_t>(rec.source.indices.size() / 3);
        for (uint32_t triIdx = 0; triIdx < triCount; ++triIdx)
            appendTri(triIdx, outIndices);
        return (outIndices.size() - beforeIndices) / 3;
    }

    bool BuildWorldTileGeometry(ExternNavmeshContext& ctx,
                                int tx,
                                int ty,
                                std::vector<glm::vec3>& outVerts,
                                std::vector<unsigned int>& outIndices,
                                bool* outAbortedByTriLimit = nullptr)
    {
        if (outAbortedByTriLimit)
            *outAbortedByTriLimit = false;
        outVerts.clear();
        outIndices.clear();

        const uint64_t tileKey = MakeTileKey(tx, ty);
        auto itGeoms = ctx.tileToGeometryIds.find(tileKey);
        if (itGeoms == ctx.tileToGeometryIds.end())
            return false;

        const dtNavMesh* nav = ctx.navData.GetNavMesh();
        if (!nav)
            return false;

        const dtNavMeshParams* params = nav->getParams();
        if (!params)
            return false;

        const float cs = std::max(0.01f, ctx.genSettings.cellSize);
        const int walkableRadius = static_cast<int>(std::ceil(ctx.genSettings.agentRadius / cs));
        const int borderSize = walkableRadius + 3;
        const float border = borderSize * cs;
        const glm::vec3 tileMin(params->orig[0] + tx * params->tileWidth - border,
                                ctx.bboxMin.y - border,
                                params->orig[2] + ty * params->tileHeight - border);
        const glm::vec3 tileMax(params->orig[0] + (tx + 1) * params->tileWidth + border,
                                ctx.bboxMax.y + border,
                                params->orig[2] + (ty + 1) * params->tileHeight + border);
        uint64_t totalRawTris = 0;
        uint64_t totalFilteredTris = 0;
        uint64_t filteredBelow0 = 0;
        uint64_t filteredY0_50 = 0;
        uint64_t filteredY50_150 = 0;
        uint64_t filteredY150Plus = 0;
        std::vector<std::pair<std::string, size_t>> perGeomAdded;
        static constexpr uint64_t MAX_INPUT_TRIS_PER_TILE = 20000000ull;

        for (const std::string& geomId : itGeoms->second)
        {
            auto it = ctx.worldGeometry.find(geomId);
            if (it == ctx.worldGeometry.end())
                continue;
            auto& rec = it->second;
            if (!rec.loaded || !rec.source.Valid())
            {
                const uint64_t mtime = GetFileMTimeHash(rec.path);
                const uint64_t fsize = GetFileSizeBytes(rec.path);
                if (mtime == 0 || fsize == 0)
                {
                    for (uint64_t k : rec.touchedTileKeys)
                    {
                        ctx.dirtyWorldTiles.insert(k);
                        ctx.dirtyWorldOffmeshTiles.insert(k);
                        EnqueueTileBuild(ctx, k);
                    }
                    continue;
                }

                if ((rec.fileMTime != 0 && rec.fileMTime != mtime) ||
                    (rec.fileSize != 0 && rec.fileSize != fsize))
                {
                    for (uint64_t k : rec.touchedTileKeys)
                    {
                        ctx.dirtyWorldTiles.insert(k);
                        ctx.dirtyWorldOffmeshTiles.insert(k);
                        EnqueueTileBuild(ctx, k);
                    }
                }

                rec.source = LoadGeometry(rec.path.c_str(), rec.preferBin);
                rec.loaded = rec.source.Valid();
                if (!rec.loaded)
                    continue;
                rec.fileMTime = mtime;
                rec.fileSize = fsize;
            }

            totalRawTris += rec.source.indices.size() / 3;

            if (rec.worldBMin.x > tileMax.x || rec.worldBMax.x < tileMin.x ||
                rec.worldBMin.y > tileMax.y || rec.worldBMax.y < tileMin.y ||
                rec.worldBMin.z > tileMax.z || rec.worldBMax.z < tileMin.z)
            {
                continue;
            }

            const size_t added = AppendGeometryForTile(rec, tileMin, tileMax, outVerts, outIndices);
            if (added > 0)
            {
                totalFilteredTris += added;
                const size_t startIdx = outIndices.size() - (added * 3);
                for (size_t i = startIdx; i + 2 < outIndices.size(); i += 3)
                {
                    const glm::vec3& a = outVerts[outIndices[i + 0]];
                    const glm::vec3& b = outVerts[outIndices[i + 1]];
                    const glm::vec3& c = outVerts[outIndices[i + 2]];
                    const float triCenterY = (a.y + b.y + c.y) / 3.0f;
                    if (triCenterY < 0.0f) ++filteredBelow0;
                    else if (triCenterY < 50.0f) ++filteredY0_50;
                    else if (triCenterY < 150.0f) ++filteredY50_150;
                    else ++filteredY150Plus;
                }
                perGeomAdded.emplace_back(rec.id, added);
                const uint64_t sourceTris = rec.source.indices.size() / 3;
                if (added > sourceTris)
                {
                    printf("[WorldTile][erro] addedTris > sourceTris tile %d,%d id=%s added=%zu source=%llu\n",
                           tx, ty, rec.id.c_str(), added, static_cast<unsigned long long>(sourceTris));
                }
                if (sourceTris > 1000000ull)
                {
                    printf("[WorldTile] sourceTris alto id=%s path=%s source=%llu\n",
                           rec.id.c_str(), rec.path.c_str(), static_cast<unsigned long long>(sourceTris));
                }
                if (added > 1000000)
                    printf("[WorldTile] Geometry triCount muito alto no tile %d,%d: id=%s path=%s tris=%zu\n",
                           tx, ty, rec.id.c_str(), rec.path.c_str(), added);
                if (totalFilteredTris > MAX_INPUT_TRIS_PER_TILE)
                {
                    printf("[WorldTile][erro] tile %d,%d excedeu limite de tris (%llu > %llu). "
                           "Marcando como failed; reduza tileSize ou simplifique geometria.\n",
                           tx, ty,
                           static_cast<unsigned long long>(totalFilteredTris),
                           static_cast<unsigned long long>(MAX_INPUT_TRIS_PER_TILE));
                    if (outAbortedByTriLimit)
                        *outAbortedByTriLimit = true;
                    outVerts.clear();
                    outIndices.clear();
                    return false;
                }
            }
        }

        std::sort(perGeomAdded.begin(), perGeomAdded.end(), [](const auto& a, const auto& b)
        {
            return a.second > b.second;
        });
        const size_t topN = std::min<size_t>(10, perGeomAdded.size());
        for (size_t i = 0; i < topN; ++i)
        {
            printf("[WorldTile] Tile %d,%d topGeom[%zu]=%s tris=%zu\n",
                   tx, ty, i, perGeomAdded[i].first.c_str(), perGeomAdded[i].second);
        }
        printf("[WorldTile] Tile %d,%d tris raw=%llu filtered=%llu geoms=%zu\n",
               tx, ty,
               static_cast<unsigned long long>(totalRawTris),
               static_cast<unsigned long long>(totalFilteredTris),
               itGeoms->second.size());
        printf("[WorldTile] Tile %d,%d filteredYDist below0=%llu y0_50=%llu y50_150=%llu y150plus=%llu\n",
               tx, ty,
               static_cast<unsigned long long>(filteredBelow0),
               static_cast<unsigned long long>(filteredY0_50),
               static_cast<unsigned long long>(filteredY50_150),
               static_cast<unsigned long long>(filteredY150Plus));

        return !outVerts.empty() && !outIndices.empty();
    }

    bool RebuildDirtyTiles(ExternNavmeshContext& ctx)
    {
        if (ctx.worldTileStreamingEnabled)
        {
            MarkTilesDirty(ctx, ctx.dirtyWorldTiles);
            ctx.dirtyBounds.clear();
            return BuildQueuedWorldTiles(&ctx, 0, 0, false) >= 0;
        }

        if (ctx.rebuildAll || !ctx.navData.IsLoaded() || !ctx.navData.HasTiledCache())
            return false;

        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        if (!CombineGeometry(ctx, verts, indices))
            return false;

        if (!ctx.navData.UpdateCachedGeometry(verts, indices))
            return false;

        std::set<uint64_t> dirtyTiles;
        const auto tileKey = [](int tx, int ty) -> uint64_t
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
        };

        for (const auto& bounds : ctx.dirtyBounds)
        {
            std::vector<std::pair<int, int>> tiles;
            if (!ctx.navData.CollectTilesInBounds(bounds.first, bounds.second, true, tiles))
                continue;
            for (const auto& t : tiles)
                dirtyTiles.insert(tileKey(t.first, t.second));
        }

        if (dirtyTiles.empty())
        {
            ctx.dirtyBounds.clear();
            return true;
        }

        std::vector<std::pair<int, int>> toRebuild;
        toRebuild.reserve(dirtyTiles.size());
        for (uint64_t key : dirtyTiles)
        {
            int tx = static_cast<int>(key >> 32);
            int ty = static_cast<int>(key & 0xffffffffu);
            toRebuild.emplace_back(tx, ty);
        }

        bool ok = ctx.navData.RebuildSpecificTiles(toRebuild, ctx.genSettings, true, nullptr);
        if (ok)
            ctx.dirtyBounds.clear();
        return ok;
    }

    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild)
    {
        if (ctx.streamingEnabled)
        {
            if (ctx.worldTileStreamingEnabled)
            {
                if (forceFullBuild || ctx.rebuildAll)
                {
                    printf("[ExternC] UpdateNavmeshState: full build ignorado em world tile streaming; use BuildQueuedWorldTiles.\n");
                }
                if (!ctx.dirtyBounds.empty())
                    ctx.dirtyBounds.clear();
                ctx.rebuildAll = false;
                return EnsureNavQuery(ctx);
            }

            if (!ctx.navData.IsLoaded() || !ctx.navData.HasTiledCache())
            {
                if (!ctx.hasBoundingBox)
                {
                    printf("[NavMeshData] UpdateNavmeshState: streaming mode sem bounds fixos.\n");
                    return false;
                }

                float forcedMin[3] = { ctx.bboxMin.x, ctx.bboxMin.y, ctx.bboxMin.z };
                float forcedMax[3] = { ctx.bboxMax.x, ctx.bboxMax.y, ctx.bboxMax.z };
                ctx.navData.SetOffmeshLinks(ctx.offmeshLinks);
                if (!ctx.navData.InitTiledGrid(ctx.genSettings, forcedMin, forcedMax))
                    return false;
            }

            if (forceFullBuild || ctx.rebuildAll)
                return BuildNavmeshInternal(ctx);

            if (!ctx.dirtyBounds.empty())
            {
                if (!RebuildDirtyTiles(ctx))
                    return false;
            }

            ctx.rebuildAll = false;
            return EnsureNavQuery(ctx);
        }

        if (forceFullBuild || ctx.rebuildAll || !ctx.navData.IsLoaded())
            return BuildNavmeshInternal(ctx);

        if (ctx.genSettings.mode != NavmeshBuildMode::Tiled || !ctx.navData.HasTiledCache())
            return BuildNavmeshInternal(ctx);

        if (!RebuildDirtyTiles(ctx))
            return BuildNavmeshInternal(ctx);

        return EnsureNavQuery(ctx);
    }


    Vector3 ToVector3(const glm::vec3& v)
    {
        return Vector3{v.x, v.y, v.z};
    }

    glm::vec3 ComputePolyNormal(const dtMeshTile* tile, const dtPoly* poly)
    {
        if (!tile || !poly || poly->vertCount < 3)
            return glm::vec3(0.0f, 1.0f, 0.0f);

        const int i0 = poly->verts[0];
        const int i1 = poly->verts[1];
        const int i2 = poly->verts[2];
        const float* v0 = &tile->verts[i0 * 3];
        const float* v1 = &tile->verts[i1 * 3];
        const float* v2 = &tile->verts[i2 * 3];
        const glm::vec3 a(v0[0], v0[1], v0[2]);
        const glm::vec3 b(v1[0], v1[1], v1[2]);
        const glm::vec3 c(v2[0], v2[1], v2[2]);
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z))
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        return normal;
    }

    glm::vec3 ComputePolyCentroid(const dtMeshTile* tile, const dtPoly* poly)
    {
        glm::vec3 sum(0.0f);
        for (int i = 0; i < poly->vertCount; ++i)
        {
            const int vi = poly->verts[i];
            const float* v = &tile->verts[vi * 3];
            sum += glm::vec3(v[0], v[1], v[2]);
        }
        if (poly->vertCount > 0)
            sum /= static_cast<float>(poly->vertCount);
        return sum;
    }

    glm::vec3 ComputeEdgeOutwardNormal(const glm::vec3& a,
                                       const glm::vec3& b,
                                       const glm::vec3& polyCenter,
                                       const glm::vec3& polyNormal)
    {
        const glm::vec3 edge = b - a;
        const float edgeLenSq = glm::dot(edge, edge);
        if (edgeLenSq <= std::numeric_limits<float>::epsilon())
            return glm::vec3(0.0f);

        const glm::vec3 edgeDir = edge / std::sqrt(edgeLenSq);
        glm::vec3 surfaceNormal = polyNormal;
        const float surfaceNormalLenSq = glm::dot(surfaceNormal, surfaceNormal);
        if (surfaceNormalLenSq <= std::numeric_limits<float>::epsilon())
            surfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            surfaceNormal /= std::sqrt(surfaceNormalLenSq);

        // Em um polígono orientado, cross(edge, normal) aponta para o lado externo.
        glm::vec3 outward = glm::cross(edgeDir, surfaceNormal);
        const float outwardLenSq = glm::dot(outward, outward);
        if (outwardLenSq <= std::numeric_limits<float>::epsilon())
            return glm::vec3(0.0f);
        outward /= std::sqrt(outwardLenSq);

        const glm::vec3 edgeCenter = (a + b) * 0.5f;
        const glm::vec3 centerToEdge = edgeCenter - polyCenter;
        const float centerToEdgeLenSq = glm::dot(centerToEdge, centerToEdge);
        if (centerToEdgeLenSq > std::numeric_limits<float>::epsilon() &&
            glm::dot(outward, centerToEdge / std::sqrt(centerToEdgeLenSq)) < 0.0f)
            outward = -outward;

        // Mantém o normal no plano XZ, como esperado pelo consumidor atual.
        outward.y = 0.0f;
        const float horizontalLenSq = outward.x * outward.x + outward.z * outward.z;
        if (horizontalLenSq <= std::numeric_limits<float>::epsilon())
            return glm::vec3(0.0f);
        outward /= std::sqrt(horizontalLenSq);

        if (!std::isfinite(outward.x) || !std::isfinite(outward.y) || !std::isfinite(outward.z))
            outward = glm::vec3(0.0f, 0.0f, 0.0f);
        return outward;
    }

    static dtPolyRef GetNeighbourRef(const dtMeshTile* tile,
                                 const dtPoly* poly,
                                 int edge,
                                 const dtNavMesh* nav)
    {
        if (!tile || !poly || !nav) return 0;
        if (edge < 0 || edge >= poly->vertCount) return 0;

        const unsigned short nei = poly->neis[edge];
        if (nei == 0)
            return 0;

        if ((nei & DT_EXT_LINK) == 0)
        {
            const unsigned int neighbourIndex = static_cast<unsigned int>(nei - 1);
            return nav->getPolyRefBase(tile) | neighbourIndex;
        }

        // EXT_LINK: retorna o primeiro link do edge (se você só quer 1)
        for (unsigned int linkIndex = poly->firstLink;
            linkIndex != DT_NULL_LINK;
            linkIndex = tile->links[linkIndex].next)
        {
            const dtLink& link = tile->links[linkIndex];
            if (link.edge == edge)
                return link.ref;
        }

        return 0;
    }
}

GTANAVVIEWER_API void* InitNavMesh()
{
    auto* ctx = new ExternNavmeshContext();
    return ctx;
}

GTANAVVIEWER_API void DestroyNavMeshResources(void* navMesh)
{
    if (!navMesh)
        return;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->navQuery)
    {
        dtFreeNavMeshQuery(ctx->navQuery);
        ctx->navQuery = nullptr;
    }

    delete ctx;
}

GTANAVVIEWER_API void SetNavMeshGenSettings(void* navMesh, const NavmeshGenerationSettings* settings)
{
    if (!navMesh || !settings)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->genSettings = *settings;
    ctx->autoOffmeshParams.agentRadius = settings->agentRadius;
    ctx->autoOffmeshParams.agentHeight = settings->agentHeight;
    ctx->autoOffmeshParams.maxSlopeDegrees = settings->agentMaxSlope;
    ctx->autoOffmeshParamsV2.agentRadius = settings->agentRadius;
    ctx->autoOffmeshParamsV2.agentHeight = settings->agentHeight;
    ctx->autoOffmeshParamsV2.maxSlopeDegrees = settings->agentMaxSlope;
    ctx->autoOffmeshParamsV2.outwardOffset = std::max(ctx->autoOffmeshParamsV2.outwardOffset, settings->agentRadius + 0.05f);
    ctx->autoOffmeshParamsV2.sweepSideOffset = std::max(0.05f, settings->agentRadius * 0.6f);
    ctx->autoOffmeshParamsV2.sweepUp = std::max(0.1f, settings->agentHeight * 0.5f);
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API void SetAutoOffMeshGenerationParams(void* navMesh, const AutoOffmeshGenerationParamsV2* params)
{
    if (!navMesh || !params) {
        printf("[ExternC] params ou navMesh inválidos.\n");
        return;
    }
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->autoOffmeshParamsV2 = *params;
}

GTANAVVIEWER_API void SetNavMeshCacheRoot(void* navMesh, const char* cacheRoot)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->cacheRoot = cacheRoot ? cacheRoot : "";
    ctx->dbIndexCache.clear();
    ctx->dbIndexLoaded = false;
    ctx->dbMTime = {};
    ctx->worldManifestLoaded = false;
}

GTANAVVIEWER_API void SetNavMeshSessionId(void* navMesh, const char* sessionId)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->sessionId = sessionId ? sessionId : "";
    ctx->dbIndexCache.clear();
    ctx->dbIndexLoaded = false;
    ctx->dbMTime = {};
}

GTANAVVIEWER_API void SetMaxResidentTiles(void* navMesh, int maxTiles)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->maxResidentTiles = std::max(1, maxTiles);
}

GTANAVVIEWER_API bool SaveNavMeshRuntimeCache(void* navMesh, const char* cacheFilePath)
{
    if (!navMesh || !cacheFilePath)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return SaveRuntimeCacheFile(*ctx, std::filesystem::path(cacheFilePath));
}

GTANAVVIEWER_API bool LoadNavMeshRuntimeCache(void* navMesh, const char* cacheFilePath)
{
    if (!navMesh || !cacheFilePath)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return LoadRuntimeCacheFile(*ctx, std::filesystem::path(cacheFilePath));
}

GTANAVVIEWER_API bool AddGeometry(void* navMesh,
                                  const char* pathToGeometry,
                                  Vector3 pos,
                                  Vector3 rot,
                                  const char* customID,
                                  bool preferBIN)
{
    if (!navMesh || !pathToGeometry || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->worldTileStreamingEnabled)
        return QueueWorldGeometry(navMesh, pathToGeometry, pos, rot, customID, preferBIN) >= 0;

    LoadedGeometry geom = LoadGeometry(pathToGeometry, preferBIN);
    if (!geom.Valid())
        return false;

    GeometryInstance inst{};
    inst.id = customID;
    inst.source = std::move(geom);
    inst.position = glm::vec3(pos.x, pos.y, pos.z);
    inst.rotation = glm::vec3(rot.x, rot.y, rot.z);
    UpdateWorldBounds(inst);

    auto* existing = FindGeometry(*ctx, customID);
    GeometryInstance* target = nullptr;
    if (existing)
    {
        *existing = std::move(inst);
        target = existing;
    }
    else
    {
        ctx->geometries.push_back(std::move(inst));
        target = &ctx->geometries.back();
    }

    RegisterDirtyBounds(*ctx, *target);
    ctx->rebuildAll = ctx->rebuildAll || ctx->genSettings.mode != NavmeshBuildMode::Tiled;
    return true;
}

GTANAVVIEWER_API bool UpdateGeometry(void* navMesh,
                                     const char* customID,
                                     const Vector3* pos,
                                     const Vector3* rot,
                                     bool updateNavMesh)
{
    if (!navMesh || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->worldTileStreamingEnabled)
    {
        auto it = ctx->worldGeometry.find(customID);
        if (it == ctx->worldGeometry.end())
            return false;

        auto& rec = it->second;
        std::unordered_set<uint64_t> oldTiles;
        auto prevIt = ctx->geomToTiles.find(customID);
        if (prevIt != ctx->geomToTiles.end())
            oldTiles = prevIt->second;
        else
            oldTiles.insert(rec.touchedTileKeys.begin(), rec.touchedTileKeys.end());

        if (!oldTiles.empty())
            MarkTilesDirty(*ctx, oldTiles);

        RemoveGeometryFromWorldIndex(*ctx, customID);

        if (pos)
            rec.position = glm::vec3(pos->x, pos->y, pos->z);
        if (rot)
            rec.rotation = glm::vec3(rot->x, rot->y, rot->z);

        rec.touchedTileKeys.clear();
        rec.indexed = false;
        rec.loaded = false;
        rec.spatialCacheBuilt = false;
        rec.transformedVertices.clear();
        rec.transformedHash = 0;
        rec.spatialCache.sourceHash = 0;
        rec.spatialCache.chunks.clear();
        rec.spatialCache.cellToChunks.clear();

        if (ctx->pendingWorldGeometrySet.insert(customID).second)
            ctx->pendingWorldGeometryQueue.push_back(customID);
        const bool isDynamic = (rec.flags & WORLD_GEOM_DYNAMIC) != 0;
        if (!isDynamic && ctx->worldAutoSaveManifest)
            SaveWorldTileManifestInternal(*ctx);
        return !updateNavMesh || EnsureNavQuery(*ctx);
    }

    auto* inst = FindGeometry(*ctx, customID);
    if (!inst)
        return false;

    glm::vec3 oldBMin = inst->worldBMin;
    glm::vec3 oldBMax = inst->worldBMax;

    if (pos)
        inst->position = glm::vec3(pos->x, pos->y, pos->z);
    if (rot)
        inst->rotation = glm::vec3(rot->x, rot->y, rot->z);

    UpdateWorldBounds(*inst);
    RegisterDirtyBounds(*ctx, *inst);

    // Also mark previous bounds to ensure tiles covering old position are refreshed
    ctx->dirtyBounds.emplace_back(oldBMin, oldBMax);

    if (updateNavMesh)
        return UpdateNavmeshState(*ctx, false);

    return true;
}

GTANAVVIEWER_API bool RemoveGeometry(void* navMesh, const char* customID)
{
    if (!navMesh || !customID)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->worldTileStreamingEnabled)
    {
        const std::string id(customID);
        bool isDynamic = false;
        auto itGeom = ctx->worldGeometry.find(id);
        if (itGeom != ctx->worldGeometry.end())
            isDynamic = (itGeom->second.flags & WORLD_GEOM_DYNAMIC) != 0;

        std::unordered_set<uint64_t> oldTiles;
        auto itTiles = ctx->geomToTiles.find(id);
        if (itTiles != ctx->geomToTiles.end())
            oldTiles = itTiles->second;
        else
        {
            auto itGeom = ctx->worldGeometry.find(id);
            if (itGeom != ctx->worldGeometry.end())
                oldTiles.insert(itGeom->second.touchedTileKeys.begin(), itGeom->second.touchedTileKeys.end());
        }
        if (!oldTiles.empty())
            MarkTilesDirty(*ctx, oldTiles);
        RemoveGeometryFromWorldIndex(*ctx, id);
        ctx->worldGeometry.erase(id);
        ctx->pendingWorldGeometrySet.erase(id);
        ctx->pendingWorldGeometryQueue.erase(
            std::remove(ctx->pendingWorldGeometryQueue.begin(), ctx->pendingWorldGeometryQueue.end(), id),
            ctx->pendingWorldGeometryQueue.end());
        if (!isDynamic && ctx->worldAutoSaveManifest)
            SaveWorldTileManifestInternal(*ctx);
        return true;
    }

    for (auto it = ctx->geometries.begin(); it != ctx->geometries.end(); ++it)
    {
        if (it->id == customID)
        {
            RegisterDirtyBounds(*ctx, *it);
            ctx->geometries.erase(it);
            ctx->rebuildAll = ctx->rebuildAll || ctx->genSettings.mode != NavmeshBuildMode::Tiled;
            return true;
        }
    }
    return false;
}

GTANAVVIEWER_API void RemoveAllGeometries(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->worldTileStreamingEnabled)
    {
        std::unordered_set<uint64_t> allTiles;
        for (const auto& kv : ctx->geomToTiles)
            allTiles.insert(kv.second.begin(), kv.second.end());
        MarkTilesDirty(*ctx, allTiles);
        ctx->worldGeometry.clear();
        ctx->pendingWorldGeometryQueue.clear();
        ctx->pendingWorldGeometrySet.clear();
        ctx->tileToGeometryIds.clear();
        ctx->geomToTiles.clear();
    }
    ctx->geometries.clear();
    ctx->rebuildAll = true;
    if (ctx->worldTileStreamingEnabled && ctx->worldAutoSaveManifest)
        SaveWorldTileManifestInternal(*ctx);
}

GTANAVVIEWER_API int GetAllGeometries(void* navMesh,
                                      NavMeshGeometryInfo* geometries,
                                      int maxGeometries,
                                      int* outGeometryCount)
{
    if (outGeometryCount)
        *outGeometryCount = 0;

    if (!navMesh)
        return 0;

    const auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    const int total = static_cast<int>(ctx->geometries.size());

    if (!geometries || maxGeometries <= 0)
    {
        if (outGeometryCount)
            *outGeometryCount = total;
        return 0;
    }

    const int count = std::min(total, maxGeometries);
    for (int i = 0; i < count; ++i)
    {
        const auto& src = ctx->geometries[static_cast<size_t>(i)];
        NavMeshGeometryInfo info{};
        std::snprintf(info.customID, sizeof(info.customID), "%s", src.id.c_str());
        info.position = Vector3{src.position.x, src.position.y, src.position.z};
        info.rotation = Vector3{src.rotation.x, src.rotation.y, src.rotation.z};
        info.vertexCount = static_cast<int>(src.source.vertices.size());
        info.indexCount = static_cast<int>(src.source.indices.size());
        geometries[i] = info;
    }

    if (outGeometryCount)
        *outGeometryCount = total;

    return count;
}

GTANAVVIEWER_API bool ExportMergedGeometriesObj(void* navMesh, const char* outputObjPath)
{
    if (!navMesh || !outputObjPath)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled && ctx->geometries.empty())
        return false;

    std::filesystem::path outPath(outputObjPath);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path());

    std::ofstream out(outPath);
    if (!out.is_open())
        return false;

    out << "# Merged navmesh geometries\n";

    std::size_t globalVertexOffset = 1;
    if (!ctx->worldTileStreamingEnabled)
    {
        for (const auto& geom : ctx->geometries)
        {
            std::ostringstream comment;
            comment << "# position " << geom.position.x << " " << geom.position.y << " " << geom.position.z << "\n";
            comment << "# rotationDeg " << geom.rotation.x << " " << geom.rotation.y << " " << geom.rotation.z << "\n";
            (void)WriteGeometryObj(out,
                                   geom.source,
                                   geom.position,
                                   geom.rotation,
                                   "geometry_" + geom.id,
                                   comment.str(),
                                   globalVertexOffset);
        }
        return out.good();
    }

    std::size_t considered = 0;
    std::size_t exported = 0;
    std::size_t filtered = 0;
    std::size_t loadFailures = 0;
    std::size_t totalVertices = 0;
    std::size_t totalFaces = 0;
    for (auto& [id, rec] : ctx->worldGeometry)
    {
        (void)id;
        ++considered;
        if (!rec.loaded || !rec.source.Valid())
        {
            rec.source = LoadGeometry(rec.path.c_str(), rec.preferBin);
            rec.loaded = rec.source.Valid();
        }
        if (!rec.loaded || !rec.source.Valid())
        {
            ++loadFailures;
            ++filtered;
            printf("[ExternC] ExportMergedGeometriesObj: skip invalid world geometry id=%s path=%s\n",
                   rec.id.c_str(),
                   rec.path.c_str());
            continue;
        }

        std::ostringstream comment;
        comment << "# path " << rec.path << "\n";
        comment << "# flags " << rec.flags << "\n";
        comment << "# groupId " << rec.groupId << "\n";
        comment << "# position " << rec.position.x << " " << rec.position.y << " " << rec.position.z << "\n";
        comment << "# rotationDeg " << rec.rotation.x << " " << rec.rotation.y << " " << rec.rotation.z << "\n";

        if (!WriteGeometryObj(out,
                              rec.source,
                              rec.position,
                              rec.rotation,
                              "world_geometry_" + rec.id,
                              comment.str(),
                              globalVertexOffset))
        {
            ++filtered;
            continue;
        }

        ++exported;
        totalVertices += rec.source.vertices.size();
        totalFaces += rec.source.indices.size() / 3;
    }
    printf("[ExternC] ExportMergedGeometriesObj(world): considered=%zu exported=%zu skipped=%zu loadFailures=%zu approxVerts=%zu approxFaces=%zu\n",
           considered,
           exported,
           filtered,
           loadFailures,
           totalVertices,
           totalFaces);

    return out.good();
}

GTANAVVIEWER_API bool ExportWorldGeometriesObj(void* navMesh,
                                               const char* outputObjPath,
                                               uint32_t includeFlags,
                                               uint32_t excludeFlags,
                                               const char* groupId,
                                               bool onlyLoaded,
                                               bool onlyResidentTiles)
{
    if (!navMesh || !outputObjPath)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled)
        return false;

    std::filesystem::path outPath(outputObjPath);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path());
    std::ofstream out(outPath);
    if (!out.is_open())
        return false;
    out << "# World navmesh geometries\n";

    const std::string groupFilter = groupId ? groupId : "";
    std::size_t globalVertexOffset = 1, considered = 0, exported = 0, filtered = 0, loadFailures = 0, totalVertices = 0, totalFaces = 0;
    for (auto& [id, rec] : ctx->worldGeometry)
    {
        (void)id;
        ++considered;
        if (includeFlags != 0 && (rec.flags & includeFlags) == 0) { ++filtered; continue; }
        if (excludeFlags != 0 && (rec.flags & excludeFlags) != 0) { ++filtered; continue; }
        if (!groupFilter.empty() && rec.groupId != groupFilter) { ++filtered; continue; }
        if (onlyResidentTiles)
        {
            bool resident = false;
            if (!rec.touchedTileKeys.empty())
            {
                for (uint64_t tileKey : rec.touchedTileKeys)
                {
                    if (ctx->residentTiles.find(tileKey) != ctx->residentTiles.end()) { resident = true; break; }
                }
            }
            else
            {
                auto itTiles = ctx->geomToTiles.find(rec.id);
                if (itTiles != ctx->geomToTiles.end())
                {
                    for (uint64_t tileKey : itTiles->second)
                    {
                        if (ctx->residentTiles.find(tileKey) != ctx->residentTiles.end()) { resident = true; break; }
                    }
                }
            }
            if (!resident) { ++filtered; continue; }
        }

        if (onlyLoaded)
        {
            if (!rec.loaded || !rec.source.Valid()) { ++filtered; continue; }
        }
        else if (!rec.loaded || !rec.source.Valid())
        {
            rec.source = LoadGeometry(rec.path.c_str(), rec.preferBin);
            rec.loaded = rec.source.Valid();
        }
        if (!rec.loaded || !rec.source.Valid())
        {
            ++loadFailures; ++filtered;
            printf("[ExternC] ExportWorldGeometriesObj: skip invalid world geometry id=%s path=%s\n", rec.id.c_str(), rec.path.c_str());
            continue;
        }

        std::ostringstream comment;
        comment << "# path " << rec.path << "\n";
        comment << "# flags " << rec.flags << "\n";
        comment << "# groupId " << rec.groupId << "\n";
        comment << "# position " << rec.position.x << " " << rec.position.y << " " << rec.position.z << "\n";
        comment << "# rotationDeg " << rec.rotation.x << " " << rec.rotation.y << " " << rec.rotation.z << "\n";
        if (!WriteGeometryObj(out,
                              rec.source,
                              rec.position,
                              rec.rotation,
                              "world_geometry_" + rec.id,
                              comment.str(),
                              globalVertexOffset))
        {
            ++filtered;
            continue;
        }
        ++exported;
        totalVertices += rec.source.vertices.size();
        totalFaces += rec.source.indices.size() / 3;
    }
    printf("[ExternC] ExportWorldGeometriesObj: considered=%zu exported=%zu skipped=%zu loadFailures=%zu approxVerts=%zu approxFaces=%zu\n",
           considered, exported, filtered, loadFailures, totalVertices, totalFaces);
    return out.good();
}

GTANAVVIEWER_API bool BuildNavMesh(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->rebuildAll = true;
    return UpdateNavmeshState(*ctx, true);
}

GTANAVVIEWER_API bool UpdateNavMesh(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return UpdateNavmeshState(*ctx, false);
}

GTANAVVIEWER_API bool InitTiledGrid(void* navMesh, Vector3 bmin, Vector3 bmax)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->genSettings.mode != NavmeshBuildMode::Tiled)
        return false;

    ctx->bboxMin = glm::vec3(bmin.x, bmin.y, bmin.z);
    ctx->bboxMax = glm::vec3(bmax.x, bmax.y, bmax.z);
    ctx->hasBoundingBox = true;
    ctx->streamingEnabled = true;
    ctx->worldTileStreamingEnabled = false;
    ctx->rebuildAll = false;
    ctx->dirtyBounds.clear();

    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);

    float forcedMin[3] = { ctx->bboxMin.x, ctx->bboxMin.y, ctx->bboxMin.z };
    float forcedMax[3] = { ctx->bboxMax.x, ctx->bboxMax.y, ctx->bboxMax.z };
    if (!ctx->navData.InitTiledGrid(ctx->genSettings, forcedMin, forcedMax))
        return false;

    ctx->residentTiles.clear();
    ctx->residentStamp.clear();
    ctx->stampCounter = 0;
    ctx->worldGeometry.clear();
    ctx->pendingWorldGeometryQueue.clear();
    ctx->pendingWorldGeometrySet.clear();
    ctx->tileToGeometryIds.clear();
    ctx->geomToTiles.clear();
    ctx->dirtyWorldTiles.clear();
    ctx->pendingTileBuildQueue.clear();
    ctx->pendingTileBuildSet.clear();
    ctx->emptyWorldTiles.clear();
    ctx->emptyWorldTileHashes.clear();
    ctx->failedWorldTiles.clear();
    ctx->emptyWorldTiles.clear();
    ctx->emptyWorldTileHashes.clear();
    ctx->failedWorldTiles.clear();
    return EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API int StreamTilesAround(void* navMesh, Vector3 center, float radius, bool allowBuildIfMissing)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->worldTileStreamingEnabled)
    {
        return StreamTilesForAgents(navMesh, &center, nullptr, 1, radius, allowBuildIfMissing);
    }
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav || !ctx->navData.HasTiledCache())
        return 0;

    ctx->streamingEnabled = true;
    std::filesystem::path cachePath = GetSessionCachePath(*ctx);
    const bool hasCacheFile = std::filesystem::exists(cachePath);
    const bool indexReady = hasCacheFile && EnsureDbIndexLoaded(*ctx, cachePath);

    float cachedBMin[3];
    float cachedBMax[3];
    if (!ctx->navData.GetCachedBounds(cachedBMin, cachedBMax))
        return 0;

    const glm::vec3 centerPos(center.x, center.y, center.z);
    const glm::vec3 bmin(centerPos.x - radius, cachedBMin[1], centerPos.z - radius);
    const glm::vec3 bmax(centerPos.x + radius, cachedBMax[1], centerPos.z + radius);

    std::vector<std::pair<int, int>> tiles;
    if (!ctx->navData.CollectTilesInBounds(bmin, bmax, false, tiles))
        return 0;

    std::unordered_set<uint64_t> needed;
    needed.reserve(tiles.size() * 2);
    int loadedCount = 0;

    for (const auto& tile : tiles)
    {
        const int tx = tile.first;
        const int ty = tile.second;
        const uint64_t key = MakeTileKey(tx, ty);
        needed.insert(key);

        const bool alreadyLoaded = nav->getTileRefAt(tx, ty, 0) != 0;
        if (!alreadyLoaded)
        {
            bool loaded = false;
            if (hasCacheFile && indexReady)
            {
                if (!LoadTileFromDb(cachePath.string().c_str(), nav, tx, ty, loaded, &ctx->dbIndexCache))
                {
                    printf("[NavMeshData] StreamTilesAround: falha ao carregar tile (%d,%d) do cache.\n", tx, ty);
                }
            }
            if (!loaded && allowBuildIfMissing)
            {
                std::vector<std::pair<int, int>> rebuildTiles;
                rebuildTiles.emplace_back(tx, ty);
                if (!ctx->geometries.empty())
                {
                    if (ctx->navData.RebuildSpecificTiles(rebuildTiles, ctx->genSettings, false, nullptr))
                    {
                        loaded = nav->getTileRefAt(tx, ty, 0) != 0;
                    }
                }
            }

            if (loaded)
                ++loadedCount;
        }

        if (nav->getTileRefAt(tx, ty, 0) != 0)
        {
            ctx->residentTiles.insert(key);
            ctx->residentStamp[key] = ++ctx->stampCounter;
        }
    }

    if (ctx->residentTiles.size() > static_cast<size_t>(ctx->maxResidentTiles))
    {
        while (ctx->residentTiles.size() > static_cast<size_t>(ctx->maxResidentTiles))
        {
            uint64_t lruKey = 0;
            uint32_t lruStamp = std::numeric_limits<uint32_t>::max();
            bool found = false;

            for (const auto& entry : ctx->residentTiles)
            {
                if (needed.find(entry) != needed.end())
                    continue;
                const auto itStamp = ctx->residentStamp.find(entry);
                const uint32_t stamp = itStamp != ctx->residentStamp.end() ? itStamp->second : 0;
                if (!found || stamp < lruStamp)
                {
                    found = true;
                    lruKey = entry;
                    lruStamp = stamp;
                }
            }

            if (!found)
                break;

            const int tx = static_cast<int>(lruKey >> 32);
            const int ty = static_cast<int>(lruKey & 0xffffffffu);
            const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
            if (ref != 0)
            {
                unsigned char* tileData = nullptr;
                int tileDataSize = 0;
                const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
                if (dtStatusSucceed(status) && tileData)
                    dtFree(tileData);
            }

            ctx->residentTiles.erase(lruKey);
            ctx->residentStamp.erase(lruKey);
        }
    }

    EnsureNavQuery(*ctx);
    return loadedCount;
}

GTANAVVIEWER_API void ClearAllLoadedTiles(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return;

    const int maxTiles = nav->getMaxTiles();
    for (int i = 0; i < maxTiles; ++i)
    {
        const dtMeshTile* tile = nav->getTile(i);
        if (!tile || !tile->header)
            continue;
        const dtTileRef ref = nav->getTileRef(tile);
        if (ref == 0)
            continue;
        unsigned char* tileData = nullptr;
        int tileDataSize = 0;
        const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
        if (dtStatusSucceed(status) && tileData)
            dtFree(tileData);
    }

    ctx->residentTiles.clear();
    ctx->residentStamp.clear();
    ctx->agentResidentTiles.clear();
    ctx->stampCounter = 0;
    EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API bool BakeTilesInBounds(void* navMesh, Vector3 bmin, Vector3 bmax, bool saveToCache)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->genSettings.mode != NavmeshBuildMode::Tiled)
        return false;

    if (ctx->worldTileStreamingEnabled)
    {
        if (!ctx->navData.IsLoaded() || !ctx->navData.HasTiledCache())
            return false;

        const glm::vec3 min(bmin.x, bmin.y, bmin.z);
        const glm::vec3 max(bmax.x, bmax.y, bmax.z);
        std::vector<std::pair<int, int>> tiles;
        if (!ctx->navData.CollectTilesInBounds(min, max, false, tiles))
            return false;

        for (const auto& t : tiles)
            EnqueueTileBuild(*ctx, MakeTileKey(t.first, t.second));

        return BuildQueuedWorldTiles(navMesh, static_cast<int>(tiles.size()), 0, saveToCache) >= 0;
    }

    if (ctx->geometries.empty())
        return false;

    std::vector<glm::vec3> verts;
    std::vector<unsigned int> indices;
    if (!CombineGeometry(*ctx, verts, indices))
        return false;

    if (!ctx->navData.IsLoaded() || !ctx->navData.HasTiledCache())
    {
        const glm::vec3 useMin = ctx->hasBoundingBox ? ctx->bboxMin : glm::vec3(bmin.x, bmin.y, bmin.z);
        const glm::vec3 useMax = ctx->hasBoundingBox ? ctx->bboxMax : glm::vec3(bmax.x, bmax.y, bmax.z);

        float forcedMin[3] = { useMin.x, useMin.y, useMin.z };
        float forcedMax[3] = { useMax.x, useMax.y, useMax.z };

        ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
        if (!ctx->navData.InitTiledGrid(ctx->genSettings, forcedMin, forcedMax))
            return false;

        if (!EnsureNavQuery(*ctx))
            return false;
    }

    if (!ctx->navData.UpdateCachedGeometry(verts, indices))
        return false;

    const glm::vec3 min(bmin.x, bmin.y, bmin.z);
    const glm::vec3 max(bmax.x, bmax.y, bmax.z);
    if (!ctx->navData.RebuildTilesInBounds(min, max, ctx->genSettings, false, nullptr))
        return false;

    if (saveToCache)
    {
        std::filesystem::path cachePath = GetSessionCachePath(*ctx);
        const auto& hashes = ctx->navData.GetCachedTileHashes();
        TileDbWriteOrUpdateTiles(cachePath.string().c_str(), ctx->navData.GetNavMesh(), hashes);
        ctx->dbIndexCache.clear();
        ctx->dbIndexLoaded = false;
    }

    return true;
}

GTANAVVIEWER_API bool EnableWorldTileStreaming(void* navMesh, bool enabled)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->worldTileStreamingEnabled = enabled;
    if (enabled)
        ctx->streamingEnabled = true;
    return true;
}

GTANAVVIEWER_API bool BeginWorldTileSession(void* navMesh,
                                            Vector3 worldBMin,
                                            Vector3 worldBMax,
                                            const char* cacheRoot,
                                            const char* sessionId,
                                            int maxResidentTiles)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->genSettings.mode != NavmeshBuildMode::Tiled)
    {
        printf("[ExternC] BeginWorldTileSession: falha, modo atual nao e tiled.\n");
        return false;
    }

    if (cacheRoot)
        ctx->cacheRoot = cacheRoot;
    if (sessionId)
        ctx->sessionId = sessionId;
    ctx->maxResidentTiles = std::max(1, maxResidentTiles);
    ctx->bboxMin = glm::vec3(worldBMin.x, worldBMin.y, worldBMin.z);
    ctx->bboxMax = glm::vec3(worldBMax.x, worldBMax.y, worldBMax.z);
    ctx->hasBoundingBox = true;
    ctx->streamingEnabled = true;
    ctx->worldTileStreamingEnabled = true;
    ctx->rebuildAll = false;
    ctx->dirtyBounds.clear();
    ctx->dirtyWorldTiles.clear();
    ctx->pendingTileBuildQueue.clear();
    ctx->pendingTileBuildSet.clear();
    ctx->pendingWorldGeometryQueue.clear();
    ctx->pendingWorldGeometrySet.clear();
    ctx->tileToGeometryIds.clear();
    ctx->geomToTiles.clear();
    ctx->worldGeometry.clear();
    ctx->residentTiles.clear();
    ctx->residentStamp.clear();
    ctx->agentResidentTiles.clear();
    ctx->stampCounter = 0;
    ctx->dbIndexCache.clear();
    ctx->dbIndexLoaded = false;
    ctx->dbMTime = {};

    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    float forcedMin[3] = { ctx->bboxMin.x, ctx->bboxMin.y, ctx->bboxMin.z };
    float forcedMax[3] = { ctx->bboxMax.x, ctx->bboxMax.y, ctx->bboxMax.z };
    if (!ctx->navData.InitTiledGrid(ctx->genSettings, forcedMin, forcedMax))
        return false;

    TileGridStats stats{};
    NavMeshData::EstimateTileGrid(ctx->genSettings, forcedMin, forcedMax, stats);
    std::filesystem::path cachePath = GetSessionCachePath(*ctx);
    printf("[ExternC] BeginWorldTileSession: bounds=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) tiles=%dx%d tileWorld=%.2f cache=%s\n",
           forcedMin[0], forcedMin[1], forcedMin[2], forcedMax[0], forcedMax[1], forcedMax[2],
           stats.tileCountX, stats.tileCountY, stats.tileWorld, cachePath.string().c_str());
    const bool queryOk = EnsureNavQuery(*ctx);
    if (!queryOk)
        return false;

    if (!LoadWorldTileManifestInternal(*ctx))
    {
        printf("[WorldTile] BeginWorldTileSession: manifesto ausente/incompativel, iniciando sessao limpa.\n");
    }

    return true;
}

GTANAVVIEWER_API int QueueWorldGeometry(void* navMesh,
                                        const char* pathToGeometry,
                                        Vector3 pos,
                                        Vector3 rot,
                                        const char* customID,
                                        bool preferBIN)
{
    return QueueWorldGeometryEx(navMesh, pathToGeometry, pos, rot, customID, preferBIN, WORLD_GEOM_PERSISTENT, "default");
}

GTANAVVIEWER_API int QueueWorldGeometryEx(void* navMesh,
                                          const char* pathToGeometry,
                                          Vector3 pos,
                                          Vector3 rot,
                                          const char* customID,
                                          bool preferBIN,
                                          uint32_t flags,
                                          const char* groupId)
{
    if (!navMesh || !pathToGeometry)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled)
        return 0;

    std::string id = (customID && customID[0] != '\0')
        ? std::string(customID)
        : (std::string(pathToGeometry) + "#" + std::to_string(ctx->worldGeometry.size() + ctx->pendingWorldGeometryQueue.size()));

    bool oldWasPersistentManifestGeom = false;
    auto it = ctx->worldGeometry.find(id);
    if (it != ctx->worldGeometry.end())
    {
        std::unordered_set<uint64_t> oldTiles;
        auto prevIt = ctx->geomToTiles.find(id);
        if (prevIt != ctx->geomToTiles.end())
            oldTiles = prevIt->second;
        else
        {
            oldTiles.insert(it->second.touchedTileKeys.begin(), it->second.touchedTileKeys.end());
        }

        RemoveGeometryFromWorldIndex(*ctx, id);
        MarkTilesDirty(*ctx, oldTiles);

        oldWasPersistentManifestGeom =
            (it->second.flags & WORLD_GEOM_PERSISTENT) != 0 &&
            (it->second.flags & WORLD_GEOM_DYNAMIC) == 0;

        auto& oldRec = it->second;
        oldRec.touchedTileKeys.clear();
        oldRec.indexed = false;
        oldRec.loaded = false;
    }

    ExternNavmeshContext::WorldGeomRecord rec{};
    rec.id = id;
    rec.path = pathToGeometry;
    rec.position = glm::vec3(pos.x, pos.y, pos.z);
    rec.rotation = glm::vec3(rot.x, rot.y, rot.z);
    rec.preferBin = preferBIN;
    rec.flags = flags == 0 ? WORLD_GEOM_PERSISTENT : flags;
    if ((rec.flags & WORLD_GEOM_DYNAMIC) != 0)
        rec.flags &= ~WORLD_GEOM_PERSISTENT;
    const bool isDynamic = (rec.flags & WORLD_GEOM_DYNAMIC) != 0;
    rec.groupId = (groupId && groupId[0] != '\0') ? std::string(groupId) : "default";
    ctx->worldGeometry[id] = std::move(rec);
    if (ctx->pendingWorldGeometrySet.insert(id).second)
        ctx->pendingWorldGeometryQueue.push_back(id);
    if ((oldWasPersistentManifestGeom || !isDynamic) && ctx->worldAutoSaveManifest)
        SaveWorldTileManifestInternal(*ctx);

    return static_cast<int>(ctx->pendingWorldGeometryQueue.size());
}

GTANAVVIEWER_API bool RemoveWorldGeometryGroup(void* navMesh, const char* groupId, bool rebuildOrQueue)
{
    if (!navMesh || !groupId)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled)
        return false;

    const std::string targetGroup(groupId);
    std::vector<std::string> ids;
    std::unordered_set<uint64_t> affectedTiles;
    for (const auto& kv : ctx->worldGeometry)
    {
        if (kv.second.groupId == targetGroup)
        {
            ids.push_back(kv.first);
            auto itTiles = ctx->geomToTiles.find(kv.first);
            if (itTiles != ctx->geomToTiles.end())
                affectedTiles.insert(itTiles->second.begin(), itTiles->second.end());
        }
    }
    if (ids.empty())
        return false;

    for (const auto& id : ids)
    {
        RemoveGeometryFromWorldIndex(*ctx, id);
        ctx->worldGeometry.erase(id);
        ctx->pendingWorldGeometrySet.erase(id);
        ctx->pendingWorldGeometryQueue.erase(
            std::remove(ctx->pendingWorldGeometryQueue.begin(), ctx->pendingWorldGeometryQueue.end(), id),
            ctx->pendingWorldGeometryQueue.end());
    }

    MarkTilesDirty(*ctx, affectedTiles);
    if (rebuildOrQueue)
        BuildQueuedWorldTiles(navMesh, static_cast<int>(affectedTiles.size()), 0, true);

    if (ctx->worldAutoSaveManifest)
        SaveWorldTileManifestInternal(*ctx);
    return true;
}

GTANAVVIEWER_API int ProcessQueuedWorldGeometry(void* navMesh, int maxItems, int maxMilliseconds)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled || !ctx->navData.HasTiledCache())
        return 0;

    const int maxCount = maxItems <= 0 ? std::numeric_limits<int>::max() : maxItems;
    const auto start = std::chrono::steady_clock::now();
    int processed = 0;
    int indexed = 0;
    while (!ctx->pendingWorldGeometryQueue.empty() && processed < maxCount)
    {
        if (maxMilliseconds > 0)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds)
                break;
        }

        const std::string geomId = ctx->pendingWorldGeometryQueue.front();
        ctx->pendingWorldGeometryQueue.erase(ctx->pendingWorldGeometryQueue.begin());
        ctx->pendingWorldGeometrySet.erase(geomId);

        auto it = ctx->worldGeometry.find(geomId);
        if (it == ctx->worldGeometry.end())
            continue;

        RemoveGeometryFromWorldIndex(*ctx, geomId);

        auto& rec = it->second;
        rec.source = LoadGeometry(rec.path.c_str(), rec.preferBin);
        rec.loaded = rec.source.Valid();
        if (!rec.loaded)
        {
            printf("[ExternC] ProcessQueuedWorldGeometry: falha ao carregar %s (%s)\n", geomId.c_str(), rec.path.c_str());
            ++processed;
            continue;
        }

        GeometryInstance temp{};
        temp.position = rec.position;
        temp.rotation = rec.rotation;
        temp.source = rec.source;
        UpdateWorldBounds(temp);
        rec.worldBMin = temp.worldBMin;
        rec.worldBMax = temp.worldBMax;
        rec.fileMTime = GetFileMTimeHash(rec.path);
        rec.fileSize = GetFileSizeBytes(rec.path);
        rec.geomHash = ComputeWorldGeometryHash(rec.path, rec.position, rec.rotation, rec.worldBMin, rec.worldBMax);
        rec.indexed = false;
        rec.touchedTileKeys.clear();

        std::vector<std::pair<int, int>> tiles;
        if (ctx->navData.CollectTilesInBounds(rec.worldBMin, rec.worldBMax, false, tiles))
        {
            auto& geomTiles = ctx->geomToTiles[geomId];
            for (const auto& t : tiles)
            {
                const uint64_t tileKey = MakeTileKey(t.first, t.second);
                auto& vec = ctx->tileToGeometryIds[tileKey];
                if (std::find(vec.begin(), vec.end(), geomId) == vec.end())
                    vec.push_back(geomId);
                geomTiles.insert(tileKey);
                rec.touchedTileKeys.push_back(tileKey);
                ctx->dirtyWorldTiles.insert(tileKey);
                ctx->dirtyWorldOffmeshTiles.insert(tileKey);
                ctx->emptyWorldTiles.erase(tileKey);
                ctx->emptyWorldTileHashes.erase(tileKey);
                ctx->failedWorldTiles.erase(tileKey);
                EnqueueTileBuild(*ctx, tileKey);
            }
            rec.indexed = true;
            ++indexed;
        }

        ++processed;
    }

    printf("[ExternC] ProcessQueuedWorldGeometry: processed=%d pending=%zu indexed=%d dirtyTiles=%zu\n",
           processed, ctx->pendingWorldGeometryQueue.size(), indexed, ctx->dirtyWorldTiles.size());
    if (processed > 0 && ctx->worldAutoSaveManifest)
        SaveWorldTileManifestInternal(*ctx);
    return processed;
}

static void UnloadProcessedNonResidentTiles(ExternNavmeshContext& ctx,
                                            const std::unordered_set<uint64_t>& tileKeys)
{
    dtNavMesh* nav = ctx.navData.GetNavMesh();
    if (!nav)
        return;

    for (uint64_t key : tileKeys)
    {
        // Se está marcado como residente, não descarrega.
        if (ctx.residentTiles.find(key) != ctx.residentTiles.end())
            continue;

        const int tx = static_cast<int>(key >> 32);
        const int ty = static_cast<int>(key & 0xffffffffu);

        const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
        if (ref == 0)
            continue;

        unsigned char* tileData = nullptr;
        int tileDataSize = 0;
        const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
        if (dtStatusSucceed(status) && tileData)
            dtFree(tileData);
    }
}

GTANAVVIEWER_API void SetWorldUnloadBuiltTilesAfterSave(void* navMesh, bool enabled)
{
    if (!navMesh)
        return;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->worldUnloadBuiltTilesAfterSave = enabled;
}

GTANAVVIEWER_API int BuildQueuedWorldTiles(void* navMesh, int maxTiles, int maxMilliseconds, bool saveToCache)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (!ctx->worldTileStreamingEnabled || !ctx->navData.HasTiledCache())
        return 0;

    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return 0;

    const int maxCount = maxTiles <= 0 ? std::numeric_limits<int>::max() : maxTiles;
    const auto start = std::chrono::steady_clock::now();
    int built = 0;
    int emptied = 0;
    int failed = 0;
    std::unordered_set<uint64_t> processedTileKeys;
    std::unordered_set<uint64_t> tilesToSave;

    while (!ctx->pendingTileBuildQueue.empty() && built < maxCount)
    {
        if (maxMilliseconds > 0)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds)
                break;
        }

        const uint64_t tileKey = ctx->pendingTileBuildQueue.front();
        ctx->pendingTileBuildQueue.pop_front();
        ctx->pendingTileBuildSet.erase(tileKey);
        ctx->dirtyWorldTiles.erase(tileKey);
        processedTileKeys.insert(tileKey);

        const int tx = static_cast<int>(tileKey >> 32);
        const int ty = static_cast<int>(tileKey & 0xffffffffu);
        std::vector<glm::vec3> verts;
        std::vector<unsigned int> indices;
        bool abortedByTriLimit = false;
        const bool hasGeom = BuildWorldTileGeometry(*ctx, tx, ty, verts, indices, &abortedByTriLimit);
        const uint64_t worldHash = ComputeWorldTileHash(*ctx, tx, ty);
        if (!hasGeom)
        {
            if (abortedByTriLimit)
            {
                ++failed;
                ++built;
                ctx->failedWorldTiles.insert(tileKey);
                continue;
            }
            const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
            if (ref != 0)
            {
                unsigned char* tileData = nullptr;
                int tileDataSize = 0;
                const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
                if (dtStatusSucceed(status) && tileData)
                    dtFree(tileData);
            }
            ++emptied;
            ++built;
            tilesToSave.insert(tileKey);
            ctx->emptyWorldTiles.insert(tileKey);
            ctx->emptyWorldTileHashes[tileKey] = worldHash;
            ctx->failedWorldTiles.erase(tileKey);
            printf("[WorldTile] Build tile %d,%d geomCount=0 triCount=0 empty=1 hash=%llu\n",
                   tx, ty, static_cast<unsigned long long>(worldHash));
            continue;
        }

        bool builtTile = false;
        bool emptyTile = false;
        if (!ctx->navData.RebuildSingleTileFromGeometry(tx, ty, verts, indices, ctx->genSettings, nullptr, worldHash, &builtTile, &emptyTile))
        {
            ++failed;
            ctx->failedWorldTiles.insert(tileKey);
        }
        else if (!emptyTile)
        {
            std::vector<OffmeshLink> tileLinks;
            if (ctx->worldAutoGenerateOffmeshLinks && (ctx->dirtyWorldOffmeshTiles.count(tileKey) > 0))
            {
                GenerateWorldOffmeshLinksForTile(*ctx, tx, ty, ctx->autoOffmeshParamsV2, tileLinks);
                if (!tileLinks.empty())
                    ctx->worldOffmeshLinksByTile[tileKey] = tileLinks;
                else
                    ctx->worldOffmeshLinksByTile.erase(tileKey);
                ctx->dirtyWorldOffmeshTiles.erase(tileKey);
            }

            if (!tileLinks.empty())
            {
                bool builtWithLinks = false;
                bool emptyWithLinks = false;
                if (!ctx->navData.RebuildSingleTileFromGeometry(tx, ty, verts, indices, ctx->genSettings, &tileLinks, worldHash, &builtWithLinks, &emptyWithLinks))
                {
                    ++failed;
                    ctx->failedWorldTiles.insert(tileKey);
                }
                else
                {
                    builtTile = builtWithLinks;
                    emptyTile = emptyWithLinks;
                }
            }
        }

        if (emptyTile)
        {
            tilesToSave.insert(tileKey);
            ctx->emptyWorldTiles.insert(tileKey);
            ctx->worldOffmeshLinksByTile.erase(tileKey);
            ctx->emptyWorldTileHashes[tileKey] = worldHash;
            ctx->failedWorldTiles.erase(tileKey);
        }
        else if (builtTile)
        {
            tilesToSave.insert(tileKey);
            ctx->emptyWorldTiles.erase(tileKey);
            ctx->emptyWorldTileHashes.erase(tileKey);
            ctx->failedWorldTiles.erase(tileKey);
        }

        ++built;
        printf("[WorldTile] Build tile %d,%d geomCount=%zu triCount=%zu built=%d failed=%d hash=%llu\n",
               tx, ty,
               ctx->tileToGeometryIds[tileKey].size(),
               indices.size() / 3,
               builtTile ? 1 : 0,
               (!builtTile && !emptyTile) ? 1 : 0,
               static_cast<unsigned long long>(worldHash));
    }

    if (saveToCache && !tilesToSave.empty() && built > 0)
    {
        std::filesystem::path cachePath = GetSessionCachePath(*ctx);
        const auto& hashes = ctx->navData.GetCachedTileHashes();

        printf("[WorldTile] TileDb merge write: %s (tiles=%zu)\n",
            cachePath.string().c_str(), tilesToSave.size());

        TileDbMergeWriteOrUpdateTiles(
            cachePath.string().c_str(),
            ctx->navData.GetNavMesh(),
            hashes,
            &tilesToSave
        );

        ctx->dbIndexCache.clear();
        ctx->dbIndexLoaded = false;

        if (ctx->worldUnloadBuiltTilesAfterSave)
            UnloadProcessedNonResidentTiles(*ctx, tilesToSave);

        printf("[ExternC] BuildQueuedWorldTiles: cache salvo em %s\n",
            cachePath.string().c_str());
    }

    EnsureNavQuery(*ctx);

    printf("[WorldTile] BuildQueuedWorldTiles: processed=%zu tilesToSave=%zu failed=%d saveToCache=%d\n",
        processedTileKeys.size(), tilesToSave.size(), failed, saveToCache ? 1 : 0);

    printf("[ExternC] BuildQueuedWorldTiles: built=%d empty=%d failed=%d saved=%d cache=%s\n",
        built, emptied, failed, saveToCache ? 1 : 0, GetSessionCachePath(*ctx).string().c_str());

    if (built > 0 && ctx->worldAutoSaveManifest)
        SaveWorldTileManifestInternal(*ctx);

    return built;
}

GTANAVVIEWER_API bool SetWorldAutoOffmeshEnabled(void* navMesh, bool enabled)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->worldAutoGenerateOffmeshLinks = enabled;
    return true;
}

GTANAVVIEWER_API int GenerateWorldOffmeshLinksForQueuedTiles(void* navMesh, int maxTiles, int maxMilliseconds)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    const int maxCount = maxTiles <= 0 ? std::numeric_limits<int>::max() : maxTiles;
    const auto start = std::chrono::steady_clock::now();
    int processed = 0;
    int totalLinks = 0;
    for (auto it = ctx->dirtyWorldOffmeshTiles.begin(); it != ctx->dirtyWorldOffmeshTiles.end() && processed < maxCount;)
    {
        if (maxMilliseconds > 0)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds)
                break;
        }
        const uint64_t tileKey = *it;
        it = ctx->dirtyWorldOffmeshTiles.erase(it);
        const int tx = static_cast<int>(tileKey >> 32);
        const int ty = static_cast<int>(tileKey & 0xffffffffu);
        std::vector<OffmeshLink> links;
        if (GenerateWorldOffmeshLinksForTile(*ctx, tx, ty, ctx->autoOffmeshParamsV2, links))
        {
            if (!links.empty())
            {
                ctx->worldOffmeshLinksByTile[tileKey] = links;
                std::vector<glm::vec3> verts;
                std::vector<unsigned int> indices;
                bool abortedByTriLimit = false;
                if (BuildWorldTileGeometry(*ctx, tx, ty, verts, indices, &abortedByTriLimit))
                {
                    bool rebuilt = false;
                    bool empty = false;
                    const uint64_t worldHash = ComputeWorldTileHash(*ctx, tx, ty);
                    ctx->navData.RebuildSingleTileFromGeometry(tx, ty, verts, indices, ctx->genSettings, &links, worldHash, &rebuilt, &empty);
                }
            }
            else ctx->worldOffmeshLinksByTile.erase(tileKey);
            totalLinks += static_cast<int>(links.size());
        }
        ++processed;
    }
    printf("[WorldOffmesh] batch tiles=%d links=%d ms=%.2f\n", processed, totalLinks,
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    return processed;
}

GTANAVVIEWER_API int GetWorldOffmeshStats(void* navMesh, int* outTilesWithLinks, int* outTotalLinks, int* outDirtyOffmeshTiles)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    int total = 0;
    for (const auto& kv : ctx->worldOffmeshLinksByTile)
        total += static_cast<int>(kv.second.size());
    if (outTilesWithLinks) *outTilesWithLinks = static_cast<int>(ctx->worldOffmeshLinksByTile.size());
    if (outTotalLinks) *outTotalLinks = total;
    if (outDirtyOffmeshTiles) *outDirtyOffmeshTiles = static_cast<int>(ctx->dirtyWorldOffmeshTiles.size());
    return total;
}

GTANAVVIEWER_API int StreamTilesForAgents(void* navMesh,
                                          const Vector3* positions,
                                          const std::uint32_t* agentIds,
                                          int agentCount,
                                          float radius,
                                          bool allowBuildIfMissing)
{
    if (!navMesh || !positions || agentCount <= 0)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav || !ctx->navData.HasTiledCache())
        return 0;

    std::filesystem::path cachePath = GetSessionCachePath(*ctx);
    const bool hasCacheFile = std::filesystem::exists(cachePath);
    const bool indexReady = hasCacheFile && EnsureDbIndexLoaded(*ctx, cachePath);

    float cachedBMin[3];
    float cachedBMax[3];
    if (!ctx->navData.GetCachedBounds(cachedBMin, cachedBMax))
        return 0;

    std::unordered_set<uint64_t> needed;
    std::unordered_set<uint64_t> neededGlobal;
    int loadedFromDb = 0;
    int updatedResident = 0;
    int enqueuedBuild = 0;

    for (int i = 0; i < agentCount; ++i)
    {
        std::unordered_set<uint64_t> neededForAgent;
        const glm::vec3 center(positions[i].x, positions[i].y, positions[i].z);
        const glm::vec3 bmin(center.x - radius, cachedBMin[1], center.z - radius);
        const glm::vec3 bmax(center.x + radius, cachedBMax[1], center.z + radius);
        std::vector<std::pair<int, int>> tiles;
        if (!ctx->navData.CollectTilesInBounds(bmin, bmax, false, tiles))
            continue;
        for (const auto& t : tiles)
        {
            const uint64_t key = MakeTileKey(t.first, t.second);
            needed.insert(key);
            neededForAgent.insert(key);
        }

        if (agentIds)
            ctx->agentResidentTiles[agentIds[i]] = std::move(neededForAgent);
    }

    if (agentIds)
    {
        for (const auto& entry : ctx->agentResidentTiles)
            neededGlobal.insert(entry.second.begin(), entry.second.end());
    }
    else
    {
        neededGlobal = needed;
    }

    for (uint64_t key : neededGlobal)
    {
        const int tx = static_cast<int>(key >> 32);
        const int ty = static_cast<int>(key & 0xffffffffu);
        const bool alreadyLoaded = nav->getTileRefAt(tx, ty, 0) != 0;
        if (!alreadyLoaded)
        {
            bool loaded = false;
            bool shouldBuild = false;

            const uint64_t computedHash = ComputeWorldTileHash(*ctx, tx, ty);

            const auto itEmpty = ctx->emptyWorldTileHashes.find(key);
            const bool knownEmpty =
                ctx->emptyWorldTiles.find(key) != ctx->emptyWorldTiles.end() &&
                itEmpty != ctx->emptyWorldTileHashes.end() &&
                itEmpty->second == computedHash;

            const bool knownFailed =
                ctx->failedWorldTiles.find(key) != ctx->failedWorldTiles.end();

            if (knownEmpty || knownFailed)
                continue;

            if (hasCacheFile && indexReady)
            {
                auto itDb = ctx->dbIndexCache.find(key);
                if (itDb != ctx->dbIndexCache.end())
                {
                    if (itDb->second.geomHash != 0 && itDb->second.geomHash != computedHash)
                        shouldBuild = true;
                    else
                        LoadTileFromDb(cachePath.string().c_str(), nav, tx, ty, loaded, &ctx->dbIndexCache);
                }
                else
                {
                    shouldBuild = true;
                }
            }
            else
            {
                shouldBuild = true;
            }

            if (loaded)
                ++loadedFromDb;
            else if (shouldBuild && allowBuildIfMissing && ctx->worldTileStreamingEnabled)
            {
                EnqueueTileBuild(*ctx, key);
                ++enqueuedBuild;
            }
        }

        if (nav->getTileRefAt(tx, ty, 0) != 0)
        {
            ctx->residentTiles.insert(key);
            ctx->residentStamp[key] = ++ctx->stampCounter;
            ++updatedResident;
        }
    }

    // 1) descarrega tudo que nenhum agent precisa mais
    int unloaded = 0;
    std::vector<uint64_t> toUnload;
    for (uint64_t key : ctx->residentTiles)
    {
        if (neededGlobal.find(key) == neededGlobal.end())
            toUnload.push_back(key);
    }

    for (uint64_t key : toUnload)
    {
        const int tx = static_cast<int>(key >> 32);
        const int ty = static_cast<int>(key & 0xffffffffu);

        const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
        if (ref != 0)
        {
            unsigned char* tileData = nullptr;
            int tileDataSize = 0;
            const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);

            if (dtStatusSucceed(status))
            {
                unloaded++; // conta sempre que removeu com sucesso
                if (tileData)
                    dtFree(tileData);
            } else
            {
                printf("[StreamTiles] Falha ao remover tile (%d,%d) status=0x%x\n", tx, ty, status);
            }
        }

        ctx->residentTiles.erase(key);
        ctx->residentStamp.erase(key);
    }

    // 2) depois, LRU só como limite de segurança
    while (ctx->residentTiles.size() > static_cast<size_t>(ctx->maxResidentTiles))
    {
        uint64_t lruKey = 0;
        uint32_t lruStamp = std::numeric_limits<uint32_t>::max();
        bool found = false;
        for (uint64_t entry : ctx->residentTiles)
        {
            if (neededGlobal.find(entry) != neededGlobal.end())
                continue;
            const auto itStamp = ctx->residentStamp.find(entry);
            const uint32_t stamp = itStamp != ctx->residentStamp.end() ? itStamp->second : 0;
            if (!found || stamp < lruStamp)
            {
                found = true;
                lruStamp = stamp;
                lruKey = entry;
            }
        }
        if (!found)
            break;
        const int tx = static_cast<int>(lruKey >> 32);
        const int ty = static_cast<int>(lruKey & 0xffffffffu);
        const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
        if (ref != 0)
        {
            unsigned char* tileData = nullptr;
            int tileDataSize = 0;
            const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
            if (dtStatusSucceed(status) && tileData)
                dtFree(tileData);
        }
        ctx->residentTiles.erase(lruKey);
        ctx->residentStamp.erase(lruKey);
    }

    EnsureNavQuery(*ctx);
    printf("[ExternC] StreamTilesForAgents: agents=%d neededCurrent=%zu neededGlobal=%zu loadedFromDb=%d enqueuedBuild=%d alreadyResident=%d unloaded=%d residentTiles=%zu\n",
       agentCount, needed.size(), neededGlobal.size(), loadedFromDb, enqueuedBuild, updatedResident, unloaded, ctx->residentTiles.size());
    return static_cast<int>(needed.size());
}

GTANAVVIEWER_API void RemoveStreamingAgent(void* navMesh, std::uint32_t agentId)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return;

    ctx->agentResidentTiles.erase(agentId);
    std::unordered_set<uint64_t> neededGlobal;
    for (const auto& entry : ctx->agentResidentTiles)
        neededGlobal.insert(entry.second.begin(), entry.second.end());

    std::vector<uint64_t> toUnload;
    toUnload.reserve(ctx->residentTiles.size());
    for (uint64_t key : ctx->residentTiles)
    {
        if (neededGlobal.find(key) == neededGlobal.end())
            toUnload.push_back(key);
    }

    for (uint64_t key : toUnload)
    {
        const int tx = static_cast<int>(key >> 32);
        const int ty = static_cast<int>(key & 0xffffffffu);
        const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
        if (ref != 0)
        {
            unsigned char* tileData = nullptr;
            int tileDataSize = 0;
            const dtStatus status = nav->removeTile(ref, &tileData, &tileDataSize);
            if (dtStatusSucceed(status) && tileData)
                dtFree(tileData);
        }
        ctx->residentTiles.erase(key);
        ctx->residentStamp.erase(key);
    }

    EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API void ClearStreamingAgents(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->agentResidentTiles.clear();
    ClearAllLoadedTiles(navMesh);
}

GTANAVVIEWER_API int GetWorldTileStreamingStats(void* navMesh,
                                                int* outQueuedGeometries,
                                                int* outIndexedGeometries,
                                                int* outDirtyTiles,
                                                int* outPendingBuildTiles,
                                                int* outResidentTiles)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (outQueuedGeometries)
        *outQueuedGeometries = static_cast<int>(ctx->pendingWorldGeometryQueue.size());
    if (outIndexedGeometries)
        *outIndexedGeometries = static_cast<int>(ctx->worldGeometry.size());
    if (outDirtyTiles)
        *outDirtyTiles = static_cast<int>(ctx->dirtyWorldTiles.size());
    if (outPendingBuildTiles)
        *outPendingBuildTiles = static_cast<int>(ctx->pendingTileBuildQueue.size());
    if (outResidentTiles)
        *outResidentTiles = static_cast<int>(ctx->residentTiles.size());
    return 1;
}

GTANAVVIEWER_API int GetWorldTileStreamingStatsEx(void* navMesh,
                                                  int* outQueuedGeometries,
                                                  int* outIndexedGeometries,
                                                  int* outDirtyTiles,
                                                  int* outPendingBuildTiles,
                                                  int* outResidentTiles,
                                                  int* outManifestLoaded,
                                                  int* outTileDbIndexLoaded)
{
    if (!GetWorldTileStreamingStats(navMesh,
                                    outQueuedGeometries,
                                    outIndexedGeometries,
                                    outDirtyTiles,
                                    outPendingBuildTiles,
                                    outResidentTiles))
    {
        return 0;
    }

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (outManifestLoaded)
        *outManifestLoaded = ctx->worldManifestLoaded ? 1 : 0;
    if (outTileDbIndexLoaded)
        *outTileDbIndexLoaded = ctx->dbIndexLoaded ? 1 : 0;
    return 1;
}

GTANAVVIEWER_API bool SaveWorldTileManifest(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return SaveWorldTileManifestInternal(*ctx);
}

GTANAVVIEWER_API bool LoadWorldTileManifest(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return LoadWorldTileManifestInternal(*ctx);
}

GTANAVVIEWER_API bool HasWorldTileManifest(void* navMesh)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return std::filesystem::exists(GetWorldManifestPath(*ctx));
}

GTANAVVIEWER_API bool SetWorldTileAutoSaveManifest(void* navMesh, bool enabled)
{
    if (!navMesh)
        return false;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->worldAutoSaveManifest = enabled;
    return true;
}

GTANAVVIEWER_API int GetNavMeshPolygons(void* navMesh,
                                        NavMeshPolygonInfo* polygons,
                                        int maxPolygons,
                                        Vector3* vertices,
                                        int maxVertices,
                                        NavMeshEdgeInfo* edges,
                                        int maxEdges,
                                        int* outVertexCount,
                                        int* outEdgeCount)
{
    if (outVertexCount)
        *outVertexCount = 0;
    if (outEdgeCount)
        *outEdgeCount = 0;
    if (!navMesh || !polygons || maxPolygons <= 0 || !vertices || maxVertices <= 0 || !edges || maxEdges <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return 0;

    int writtenPolygons = 0;
    int writtenVertices = 0;
    int writtenEdges = 0;
    std::uint64_t generatedId = 1;

    const int maxTiles = nav->getMaxTiles();
    for (int tileIndex = 0; tileIndex < maxTiles && writtenPolygons < maxPolygons; ++tileIndex)
    {
        const dtMeshTile* tile = nav->getTile(tileIndex);
        if (!tile || !tile->header)
            continue;

        for (int polyIndex = 0; polyIndex < tile->header->polyCount && writtenPolygons < maxPolygons; ++polyIndex)
        {
            const dtPoly* poly = &tile->polys[polyIndex];
            if (poly->getType() != DT_POLYTYPE_GROUND)
                continue;

            const dtPolyRef pref = nav->getPolyRefBase(tile) | static_cast<unsigned int>(polyIndex);
            std::uint64_t polyId = pref != 0 ? static_cast<std::uint64_t>(pref) : generatedId++;

            const glm::vec3 center = ComputePolyCentroid(tile, poly);
            const glm::vec3 normal = ComputePolyNormal(tile, poly);

            NavMeshPolygonInfo info{};
            info.polygonId = polyId;
            info.center = ToVector3(center);
            info.normal = ToVector3(normal);
            info.vertexStart = writtenVertices;
            info.vertexCount = poly->vertCount;
            info.edgeStart = writtenEdges;
            info.edgeCount = poly->vertCount;

            if (writtenVertices + poly->vertCount > maxVertices || writtenEdges + poly->vertCount > maxEdges)
                return writtenPolygons;

            for (int v = 0; v < poly->vertCount; ++v)
            {
                const int vi = poly->verts[v];
                const float* p = &tile->verts[vi * 3];
                vertices[writtenVertices + v] = Vector3{p[0], p[1], p[2]};
            }

            for (int edge = 0; edge < poly->vertCount; ++edge)
            {
                const int vaIndex = edge;
                const int vbIndex = (edge + 1) % poly->vertCount;
                const int va = poly->verts[vaIndex];
                const int vb = poly->verts[vbIndex];
                const float* a = &tile->verts[va * 3];
                const float* b = &tile->verts[vb * 3];
                const glm::vec3 pa(a[0], a[1], a[2]);
                const glm::vec3 pb(b[0], b[1], b[2]);
                NavMeshEdgeInfo e{};
                e.vertexA = ToVector3(pa);
                e.vertexB = ToVector3(pb);
                const glm::vec3 mid = (pa + pb) * 0.5f;
                e.center = ToVector3(mid);
                const glm::vec3 outward = ComputeEdgeOutwardNormal(pa, pb, center, normal);
                e.normal = ToVector3(outward);
                e.polygonId = polyId;
                edges[writtenEdges + edge] = e;
            }

            writtenVertices += poly->vertCount;
            writtenEdges += poly->vertCount;
            polygons[writtenPolygons++] = info;
        }
    }

    if (outVertexCount)
        *outVertexCount = writtenVertices;
    if (outEdgeCount)
        *outEdgeCount = writtenEdges;
    return writtenPolygons;
}

static bool HasGroundNeighbourOnEdge(const dtMeshTile* tile,
                                     const dtPoly* poly,
                                     int edge,
                                     const dtNavMesh* nav)
{
    const unsigned short nei = poly->neis[edge];
    if (nei == 0)
        return false;

    // Vizinho interno (mesmo tile)
    if ((nei & DT_EXT_LINK) == 0)
    {
        const unsigned int neighbourIndex = static_cast<unsigned int>(nei - 1);
        const dtPolyRef ref = nav->getPolyRefBase(tile) | neighbourIndex;

        const dtMeshTile* nt = nullptr;
        const dtPoly* np = nullptr;
        if (dtStatusSucceed(nav->getTileAndPolyByRef(ref, &nt, &np)) && np)
            return np->getType() == DT_POLYTYPE_GROUND;

        // ref inválida? melhor assumir "não tem vizinho ground" => borda
        return false;
    }

    // Vizinho externo (outro tile): pode haver múltiplos links no mesmo edge.
    for (unsigned int linkIndex = poly->firstLink;
         linkIndex != DT_NULL_LINK;
         linkIndex = tile->links[linkIndex].next)
    {
        const dtLink& link = tile->links[linkIndex];
        if (link.edge != edge)
            continue;

        const dtMeshTile* nt = nullptr;
        const dtPoly* np = nullptr;
        if (dtStatusSucceed(nav->getTileAndPolyByRef(link.ref, &nt, &np)) && np)
        {
            if (np->getType() == DT_POLYTYPE_GROUND)
                return true;
        }
        // se falhar, ignora esse link e continua
    }

    return false;
}

GTANAVVIEWER_API int GetNavMeshBorderEdges(void* navMesh,
                                           NavMeshEdgeInfo* edges,
                                           int maxEdges,
                                           int* outEdgeCount)
{
    if (outEdgeCount)
        *outEdgeCount = 0;

    if (!navMesh || !edges || maxEdges <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    dtNavMesh* nav = ctx->navData.GetNavMesh();
    if (!nav)
        return 0;

    int written = 0;
    const int maxTiles = nav->getMaxTiles();

    for (int tileIndex = 0; tileIndex < maxTiles && written < maxEdges; ++tileIndex)
    {
        const dtMeshTile* tile = nav->getTile(tileIndex);
        if (!tile || !tile->header)
            continue;

        for (int polyIndex = 0; polyIndex < tile->header->polyCount && written < maxEdges; ++polyIndex)
        {
            const dtPoly* poly = &tile->polys[polyIndex];
            if (poly->getType() != DT_POLYTYPE_GROUND)
                continue;

            const dtPolyRef pref = nav->getPolyRefBase(tile) | static_cast<unsigned int>(polyIndex);
            const glm::vec3 center = ComputePolyCentroid(tile, poly);
            const glm::vec3 normal = ComputePolyNormal(tile, poly);

            for (int edge = 0; edge < poly->vertCount && written < maxEdges; ++edge)
            {
                // ✅ Novo critério: "borda" == NÃO tem vizinho ground neste edge
                if (HasGroundNeighbourOnEdge(tile, poly, edge, nav))
                    continue;

                const int vaIndex = edge;
                const int vbIndex = (edge + 1) % poly->vertCount;
                const int va = poly->verts[vaIndex];
                const int vb = poly->verts[vbIndex];

                const float* a = &tile->verts[va * 3];
                const float* b = &tile->verts[vb * 3];
                const glm::vec3 pa(a[0], a[1], a[2]);
                const glm::vec3 pb(b[0], b[1], b[2]);

                NavMeshEdgeInfo e{};
                e.vertexA = ToVector3(pa);
                e.vertexB = ToVector3(pb);

                const glm::vec3 mid = (pa + pb) * 0.5f;
                e.center = ToVector3(mid);

                e.normal = ToVector3(ComputeEdgeOutwardNormal(pa, pb, center, normal));
                e.polygonId = pref != 0 ? static_cast<std::uint64_t>(pref) : 0;

                edges[written++] = e;
            }
        }
    }

    if (outEdgeCount)
        *outEdgeCount = written;

    return written;
}

static float HeadingDegXZ(const glm::vec3& from, const glm::vec3& to)
{
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    if (!std::isfinite(dx) || !std::isfinite(dz) || (dx*dx + dz*dz) < 1e-8f)
        return 0.0f;
    return glm::degrees(std::atan2(dx, dz)); // yaw em graus
}

static int RunPathfindInternal(ExternNavmeshContext& ctx,
                               const glm::vec3& start,
                               const glm::vec3& end,
                               int flags,
                               int maxPoints,
                               float minEdge,
                               float* outPath,
                               NodeInfo* outNodeInfo,
                               int options)
{
    if (!EnsureNavQuery(ctx))
        return 0;

    const float startPos[3] = { start.x, start.y, start.z };
    const float endPos[3]   = { end.x, end.y, end.z };

    dtQueryFilter filter{};
    filter.setIncludeFlags(static_cast<unsigned short>(flags));
    filter.setExcludeFlags(0);

    filter.setAreaCost(AREA_JUMP, 4.0f);
    filter.setAreaCost(AREA_DROP, 1.5f);
    filter.setAreaCost(AREA_OFFMESH, 2.0f);

    dtPolyRef startRef = 0, endRef = 0;
    float startNearest[3]{};
    float endNearest[3]{};

    if (dtStatusFailed(ctx.navQuery->findNearestPoly(startPos, ctx.cachedExtents, &filter, &startRef, startNearest)) || startRef == 0)
        return 0;
    if (dtStatusFailed(ctx.navQuery->findNearestPoly(endPos, ctx.cachedExtents, &filter, &endRef, endNearest)) || endRef == 0)
        return 0;

    dtPolyRef polys[256];
    int polyCount = 0;
    const dtStatus pathStatus = ctx.navQuery->findPath(startRef, endRef, startNearest, endNearest, &filter, polys, &polyCount, 256);
    if (dtStatusFailed(pathStatus) || polyCount == 0)
        return 0;

    std::vector<float> straight(static_cast<size_t>(maxPoints) * 3);
    std::vector<unsigned char> straightFlags(static_cast<size_t>(maxPoints));
    std::vector<dtPolyRef> straightRefs(static_cast<size_t>(maxPoints));
    int straightCount = 0;

    dtStatus straightStatus = DT_FAILURE;
    if (std::isfinite(minEdge) && minEdge > 0.0f)
        straightStatus = ctx.navQuery->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, options, minEdge);
    else
        straightStatus = ctx.navQuery->findStraightPath(startNearest, endNearest, polys, polyCount, straight.data(), straightFlags.data(), straightRefs.data(), &straightCount, maxPoints, options);

    if (dtStatusFailed(straightStatus) || straightCount == 0)
        return 0;

    for (int i = 0; i < straightCount; ++i)
    {
        const float px = straight[i * 3 + 0];
        const float py = straight[i * 3 + 1];
        const float pz = straight[i * 3 + 2];

        outPath[i * 3 + 0] = px;
        outPath[i * 3 + 1] = py;
        outPath[i * 3 + 2] = pz;

        if (outNodeInfo)
        {
            NodeInfo ni{};
            const unsigned char nodeFlags = straightFlags[static_cast<size_t>(i)];
            ni.isOffmeshLink = (nodeFlags & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;

            // zDifference: diferença de altura pro próximo nó (Y do Detour)
            if (i + 1 < straightCount)
            {
                const float ny = straight[(i + 1) * 3 + 1];
                ni.zDifference = (ny - py);
                ni.nextNodeIsAbove = (ni.zDifference > 0.0f);
            }
            else
            {
                ni.zDifference = 0.0f;
                ni.nextNodeIsAbove = false;
            }

            // lookAtHeading: heading sugerido pro próximo nó (ou 0 se não tiver próximo)
            if (i + 1 < straightCount)
            {
                glm::vec3 cur(px, py, pz);
                glm::vec3 nxt(straight[(i + 1) * 3 + 0],
                            straight[(i + 1) * 3 + 1],
                            straight[(i + 1) * 3 + 2]);
                ni.lookAtHeading = HeadingDegXZ(cur, nxt);
            }
            else
            {
                ni.lookAtHeading = 0.0f;
            }

            outNodeInfo[i] = ni;
        }
    }

    return straightCount;
}



    constexpr float kSoftRepathMaxSnapDist = 3.0f;

    float WrapAngleDeg(float angle)
    {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

    float Distance2DSq(const glm::vec3& a, const glm::vec3& b)
    {
        const float dx = a.x - b.x;
        const float dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    glm::vec3 CornerAt(const std::vector<float>& cornersXYZ, int idx)
    {
        return glm::vec3(cornersXYZ[static_cast<size_t>(idx) * 3 + 0],
                         cornersXYZ[static_cast<size_t>(idx) * 3 + 1],
                         cornersXYZ[static_cast<size_t>(idx) * 3 + 2]);
    }

    int FindBestCornerIndexByCorner(const glm::vec3& pos,
                                    const std::vector<float>& cornersXYZ,
                                    int cornerCount,
                                    float maxSnapDist)
    {
        if (cornerCount <= 0)
            return 0;

        int bestIndex = 0;
        float bestDistSq = std::numeric_limits<float>::max();
        for (int i = 0; i < cornerCount; ++i)
        {
            const float distSq = Distance2DSq(pos, CornerAt(cornersXYZ, i));
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestIndex = i;
            }
        }

        if (maxSnapDist > 0.0f && bestDistSq > maxSnapDist * maxSnapDist)
            return 0;

        return std::clamp(bestIndex, 0, cornerCount - 1);
    }

    int FindBestCornerIndexBySegment(const glm::vec3& pos,
                                     const std::vector<float>& cornersXYZ,
                                     int cornerCount,
                                     float maxSnapDist)
    {
        if (cornerCount <= 1)
            return FindBestCornerIndexByCorner(pos, cornersXYZ, cornerCount, maxSnapDist);

        int bestIndex = 1;
        float bestDistSq = std::numeric_limits<float>::max();

        for (int i = 0; i + 1 < cornerCount; ++i)
        {
            const glm::vec3 a = CornerAt(cornersXYZ, i);
            const glm::vec3 b = CornerAt(cornersXYZ, i + 1);
            glm::vec2 ab(b.x - a.x, b.z - a.z);
            const float abLenSq = glm::dot(ab, ab);

            float t = 0.0f;
            if (abLenSq > 1e-6f)
            {
                const glm::vec2 ap(pos.x - a.x, pos.z - a.z);
                t = std::clamp(glm::dot(ap, ab) / abLenSq, 0.0f, 1.0f);
            }

            const glm::vec3 p(a.x + (b.x - a.x) * t,
                              a.y + (b.y - a.y) * t,
                              a.z + (b.z - a.z) * t);
            const float distSq = Distance2DSq(pos, p);
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestIndex = (t >= 0.5f) ? (i + 1) : i;
            }
        }

        if (maxSnapDist > 0.0f && bestDistSq > maxSnapDist * maxSnapDist)
            return 0;

        return std::clamp(bestIndex, 0, cornerCount - 1);
    }

    float LerpAngleDeg(float fromDeg, float toDeg, float alpha)
    {
        const float t = std::clamp(alpha, 0.0f, 1.0f);
        const float delta = WrapAngleDeg(toDeg - fromDeg);
        return WrapAngleDeg(fromDeg + delta * t);
    }

    void ResetAgentPathState(SimAgentState& st)
    {
        st.cornerIndex = 0;
        st.cornerCount = 0;
        st.cornersXYZ.clear();
        st.cornerFlags.clear();
        st.pathPolys.clear();
        st.currentRef = 0;
        st.inOffmesh = false;
        st.offStart = glm::vec3(0.0f);
        st.offEnd = glm::vec3(0.0f);
        st.offT = 0.0f;
        st.offDuration = 0.0f;
        st.offType = 0;
        st.offArcHeight = 0.0f;
        st.offmeshStartCornerIndex = -1;
    }

    void ClampAgentHorizontalSpeed(const SimParamsFFI& params, SimAgentState& st)
    {
        const float maxSpeed = std::max(0.0f, (st.flags & AGENT_VEHICLE) != 0 ? params.maxSpeedForward : params.agentSpeed);
        glm::vec2 v2(st.lastVel.x, st.lastVel.z);
        const float len = glm::length(v2);
        if (len > maxSpeed && len > 1e-4f)
        {
            const float k = maxSpeed / len;
            st.lastVel.x *= k;
            st.lastVel.z *= k;
        }
    }

    bool RefreshAgentNearestPoly(ExternNavmeshContext& ctx, SimAgentState& st, int includeFlags = 0xFFFF)
    {
        if (!EnsureNavQuery(ctx))
            return false;

        dtQueryFilter filter{};
        filter.setIncludeFlags(static_cast<unsigned short>(includeFlags));
        filter.setExcludeFlags(0);

        const float p[3] = { st.pos.x, st.pos.y, st.pos.z };
        float nearest[3]{};
        dtPolyRef ref = 0;
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(p, ctx.cachedExtents, &filter, &ref, nearest)) || ref == 0)
            return false;

        st.currentRef = ref;
        st.pos = glm::vec3(nearest[0], nearest[1], nearest[2]);
        return true;
    }

    float ShapeAvoidRadius(const SimAgentState& a)
    {
        if (a.shape == SHAPE_BOX)
            return std::sqrt(a.halfX * a.halfX + a.halfZ * a.halfZ);
        return a.radius;
    }

    bool ComputePathData(ExternNavmeshContext& ctx,
                         const glm::vec3& start,
                         const glm::vec3& end,
                         int flags,
                         int maxPoints,
                         float minEdge,
                         int options,
                         std::vector<float>& outCorners,
                         std::vector<unsigned char>& outCornerFlags,
                         std::vector<dtPolyRef>& outPathPolys,
                         dtPolyRef* outStartRef,
                         float outStartNearest[3])
    {
        if (!EnsureNavQuery(ctx) || maxPoints <= 0)
            return false;

        const float startPos[3] = { start.x, start.y, start.z };
        const float endPos[3] = { end.x, end.y, end.z };

        dtQueryFilter filter{};
        filter.setIncludeFlags(static_cast<unsigned short>(flags));
        filter.setExcludeFlags(0);
        filter.setAreaCost(AREA_JUMP, 4.0f);
        filter.setAreaCost(AREA_DROP, 1.5f);
        filter.setAreaCost(AREA_OFFMESH, 2.0f);

        dtPolyRef startRef = 0, endRef = 0;
        float startNearest[3]{};
        float endNearest[3]{};
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(startPos, ctx.cachedExtents, &filter, &startRef, startNearest)) || startRef == 0)
            return false;
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(endPos, ctx.cachedExtents, &filter, &endRef, endNearest)) || endRef == 0)
            return false;

        dtPolyRef polys[256]{};
        int polyCount = 0;
        const dtStatus pathStatus = ctx.navQuery->findPath(startRef, endRef, startNearest, endNearest, &filter, polys, &polyCount, 256);
        if (dtStatusFailed(pathStatus) || polyCount == 0)
            return false;

        outPathPolys.assign(polys, polys + polyCount);

        outCorners.resize(static_cast<size_t>(maxPoints) * 3);
        outCornerFlags.resize(static_cast<size_t>(maxPoints));
        std::vector<dtPolyRef> straightRefs(static_cast<size_t>(maxPoints));
        int straightCount = 0;
        dtStatus straightStatus = DT_FAILURE;
        if (std::isfinite(minEdge) && minEdge > 0.0f)
            straightStatus = ctx.navQuery->findStraightPathMinEdgePrecise(startNearest, endNearest, polys, polyCount, outCorners.data(), outCornerFlags.data(), straightRefs.data(), &straightCount, maxPoints, options, minEdge);
        else
            straightStatus = ctx.navQuery->findStraightPath(startNearest, endNearest, polys, polyCount, outCorners.data(), outCornerFlags.data(), straightRefs.data(), &straightCount, maxPoints, options);

        if (dtStatusFailed(straightStatus) || straightCount <= 0)
            return false;

        outCorners.resize(static_cast<size_t>(straightCount) * 3);
        outCornerFlags.resize(static_cast<size_t>(straightCount));
        if (outStartRef)
            *outStartRef = startRef;
        if (outStartNearest)
        {
            outStartNearest[0] = startNearest[0];
            outStartNearest[1] = startNearest[1];
            outStartNearest[2] = startNearest[2];
        }
        return true;
    }

    float SampleAgentHeight(ExternNavmeshContext& ctx, SimAgentState& agent, const glm::vec3& pos)
    {
        if (!EnsureNavQuery(ctx))
            return pos.y;

        dtQueryFilter filter{};
        filter.setIncludeFlags(0xFFFF);
        const float p[3] = { pos.x, pos.y, pos.z };
        dtPolyRef ref = agent.currentRef;
        float nearest[3]{};
        if (ref == 0)
        {
            if (dtStatusFailed(ctx.navQuery->findNearestPoly(p, ctx.cachedExtents, &filter, &ref, nearest)) || ref == 0)
                return pos.y;
            agent.currentRef = ref;
        }

        float navY = pos.y;
        if (dtStatusSucceed(ctx.navQuery->getPolyHeight(ref, p, &navY)))
            return navY;
        return pos.y;
    }

    glm::vec3 ComputeAvoidanceForce(const SimAgentState& self,
                                    const std::vector<const SimAgentState*>& neighbors,
                                    float avoidRange,
                                    float avoidWeight)
    {
        glm::vec3 force(0.0f);
        if (avoidRange <= 0.0f || avoidWeight <= 0.0f)
            return force;

        const float selfRadius = ShapeAvoidRadius(self);
        for (const SimAgentState* other : neighbors)
        {
            if (!other || other->id == self.id)
                continue;
            if ((other->teamMask & self.avoidMask) == 0)
                continue;

            glm::vec3 delta = self.pos - other->pos;
            delta.y = 0.0f;
            const float distSq = glm::dot(delta, delta);
            const float r = selfRadius + ShapeAvoidRadius(*other);
            const float range = std::max(avoidRange, r);
            if (distSq <= 1e-6f || distSq > range * range)
                continue;

            const float dist = std::sqrt(distSq);
            const float strength = (1.0f - dist / range) * avoidWeight;
            force += (delta / dist) * strength;
        }

        return force;
    }

    int AppendJumpEvent(const SimAgentState& agent,
                        int frameIndex,
                        const glm::vec3& start,
                        const glm::vec3& end,
                        SimEventFFI* outEvents,
                        int maxEvents,
                        int eventCount,
                        float speed,
                        float durationOverride = -1.0f)
    {
        if (!outEvents || eventCount >= maxEvents)
            return eventCount;

        const glm::vec2 d2(end.x - start.x, end.z - start.z);
        const float dist2d = glm::length(d2);
        const float dy = end.y - start.y;
        std::uint8_t jumpType = 2;
        if (dy > 0.75f)
            jumpType = 0;
        else if (dy < -0.75f)
            jumpType = 1;

        SimEventFFI ev{};
        ev.agentId = agent.id;
        ev.frameIndex = static_cast<std::uint16_t>(std::max(0, frameIndex));
        ev.type = 1;
        ev.jumpType = jumpType;
        ev.start[0] = start.x; ev.start[1] = start.y; ev.start[2] = start.z;
        ev.end[0] = end.x; ev.end[1] = end.y; ev.end[2] = end.z;
        if (durationOverride > 0.0f)
        {
            ev.duration = durationOverride;
        }
        else
        {
            const float denom = std::max(speed, 0.5f);
            ev.duration = std::clamp(dist2d / denom, 0.2f, 1.5f);
        }
        outEvents[eventCount++] = ev;
        return eventCount;
    }


    bool SampleGroundAtXZ(ExternNavmeshContext& ctx,
                          SimAgentState& agent,
                          float x,
                          float z,
                          float baseY,
                          float up,
                          float down,
                          float& outY)
    {
        if (!EnsureNavQuery(ctx))
            return false;

        dtQueryFilter filter{};
        filter.setIncludeFlags(0xFFFF);
        filter.setExcludeFlags(0);

        const float halfHeight = std::max(0.5f, (up + down) * 0.5f);
        const float ext[3] = { std::max(0.25f, ctx.cachedExtents[0] * 0.1f), halfHeight, std::max(0.25f, ctx.cachedExtents[2] * 0.1f) };
        const float query[3] = { x, baseY, z };

        dtPolyRef ref = 0;
        float nearest[3]{};
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(query, ext, &filter, &ref, nearest)) || ref == 0)
            return false;

        const float topY = baseY + std::max(0.0f, up);
        const float bottomY = baseY - std::max(0.0f, down);

        float y = nearest[1];
        if (dtStatusFailed(ctx.navQuery->getPolyHeight(ref, query, &y)))
            y = nearest[1];

        if (y > topY + 1e-3f || y < bottomY - 1e-3f)
            return false;

        outY = y;
        agent.currentRef = ref;
        return true;
    }

    void ComputeYawBasis(float yawDeg, glm::vec3& outForward, glm::vec3& outRight)
    {
        const float yawRad = glm::radians(yawDeg);
        outForward = glm::normalize(glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad)));
        outRight = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), outForward));
    }

    bool FitVehicleToGround(ExternNavmeshContext& ctx,
                            SimAgentState& agent,
                            const SimParamsFFI& params,
                            float dt,
                            uint8_t& frameFlags)
    {
        (void)dt;
        const float wheelInset = std::max(0.0f, params.vehicleWheelInset);
        const float hx = std::max(0.05f, agent.halfX - wheelInset);
        const float hz = std::max(0.05f, agent.halfZ - wheelInset);

        glm::vec3 fYaw;
        glm::vec3 rYaw;
        ComputeYawBasis(agent.headingDeg, fYaw, rYaw);

        const glm::vec2 offsets[4] = {
            { +hx, +hz },
            { +hx, -hz },
            { -hx, +hz },
            { -hx, -hz }
        };

        glm::vec3 wheelPos[4]{};
        float wheelY[4]{};
        bool hits[4]{};
        int hitCount = 0;
        const float up = std::max(0.0f, params.vehicleSampleUp);
        const float down = std::max(0.0f, params.vehicleSampleDown);
        const float baseY = agent.pos.y + up;

        for (int i = 0; i < 4; ++i)
        {
            const glm::vec3 worldOffset = rYaw * offsets[i].x + fYaw * offsets[i].y;
            const float wx = agent.pos.x + worldOffset.x;
            const float wz = agent.pos.z + worldOffset.z;
            wheelPos[i] = glm::vec3(wx, agent.pos.y, wz);
            hits[i] = SampleGroundAtXZ(ctx, agent, wx, wz, baseY, up, down, wheelY[i]);
            if (hits[i])
            {
                wheelPos[i].y = wheelY[i];
                ++hitCount;
            }
        }

        if (hitCount < 2)
        {
            frameFlags |= SIM_FRAMEFLAG_AIRBORNE;
            return false;
        }

        if (hitCount == 4)
            frameFlags |= SIM_FRAMEFLAG_GROUND_FIT_OK;
        else
            frameFlags |= SIM_FRAMEFLAG_GROUND_FIT_PARTIAL;

        auto fallbackY = [&](int a, int b, int c) {
            int n = 0;
            float sum = 0.0f;
            if (hits[a]) { sum += wheelPos[a].y; ++n; }
            if (hits[b]) { sum += wheelPos[b].y; ++n; }
            if (hits[c]) { sum += wheelPos[c].y; ++n; }
            return n > 0 ? (sum / static_cast<float>(n)) : agent.pos.y;
        };

        if (!hits[0]) wheelPos[0].y = fallbackY(1,2,3);
        if (!hits[1]) wheelPos[1].y = fallbackY(0,2,3);
        if (!hits[2]) wheelPos[2].y = fallbackY(0,1,3);
        if (!hits[3]) wheelPos[3].y = fallbackY(0,1,2);

        const glm::vec3 frontMid = (wheelPos[0] + wheelPos[1]) * 0.5f;
        const glm::vec3 rearMid = (wheelPos[2] + wheelPos[3]) * 0.5f;
        const glm::vec3 leftMid = (wheelPos[0] + wheelPos[2]) * 0.5f;
        const glm::vec3 rightMid = (wheelPos[1] + wheelPos[3]) * 0.5f;

        glm::vec3 vf = frontMid - rearMid;
        glm::vec3 vr = rightMid - leftMid;
        if (glm::dot(vf, vf) < 1e-6f)
            vf = fYaw;
        else
            vf = glm::normalize(vf);
        if (glm::dot(vr, vr) < 1e-6f)
            vr = rYaw;
        else
            vr = glm::normalize(vr);

        glm::vec3 upVec = glm::cross(vr, vf);
        if (glm::dot(upVec, upVec) < 1e-6f)
            upVec = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            upVec = glm::normalize(upVec);
        if (upVec.y < 0.0f)
            upVec = -upVec;

        float targetPitch = glm::degrees(std::atan2(glm::dot(fYaw, upVec), glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), upVec)));
        float targetRoll = -glm::degrees(std::atan2(glm::dot(rYaw, upVec), glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), upVec)));
        targetPitch = std::clamp(targetPitch, -std::max(0.0f, params.vehicleMaxPitchDeg), std::max(0.0f, params.vehicleMaxPitchDeg));
        targetRoll = std::clamp(targetRoll, -std::max(0.0f, params.vehicleMaxRollDeg), std::max(0.0f, params.vehicleMaxRollDeg));

        const float rotAlpha = std::clamp(params.vehicleRotAlpha, 0.0f, 1.0f);
        agent.pitchDeg = agent.pitchDeg + (targetPitch - agent.pitchDeg) * rotAlpha;
        agent.rollDeg = agent.rollDeg + (targetRoll - agent.rollDeg) * rotAlpha;

        const float minY = std::min(std::min(wheelPos[0].y, wheelPos[1].y), std::min(wheelPos[2].y, wheelPos[3].y));
        const float targetY = minY + params.vehicleGroundUpOffset;
        const float suspAlpha = std::clamp(params.vehicleSuspensionAlpha, 0.0f, 1.0f);
        agent.pos.y = agent.pos.y + (targetY - agent.pos.y) * suspAlpha;
        agent.verticalVel = 0.0f;

        agent.eulerRPYDeg = glm::vec3(agent.rollDeg, agent.pitchDeg, agent.headingDeg);
        return true;
    }

    PathAvoidParamsFFI GetDefaultAvoidParams()
    {
        PathAvoidParamsFFI p{};
        p.inflate = 0.3f;
        p.detourSideStep = 2.0f;
        p.maxDetourCandidates = 6;
        p.maxObstaclesToCheck = 64;
        p.maxFixIterations = 8;
        p.useHeightFilter = true;
        p.heightTolerance = 2.5f;
        return p;
    }

    float Dist2PointSegmentXZ(const glm::vec3& a, const glm::vec3& b, const glm::vec2& p, float* outT = nullptr)
    {
        const glm::vec2 ab(b.x - a.x, b.z - a.z);
        const glm::vec2 ap(p.x - a.x, p.y - a.z);
        const float denom = glm::dot(ab, ab);
        float t = 0.0f;
        if (denom > 1e-6f)
            t = std::clamp(glm::dot(ap, ab) / denom, 0.0f, 1.0f);
        const glm::vec2 c(a.x + ab.x * t, a.z + ab.y * t);
        const glm::vec2 d = p - c;
        if (outT)
            *outT = t;
        return glm::dot(d, d);
    }

    bool SegmentIntersectsCircleXZ(const glm::vec3& a, const glm::vec3& b, const DynObstacleState& o, float inflate, float* outT)
    {
        const float r = std::max(0.01f, o.radius + inflate);
        const float d2 = Dist2PointSegmentXZ(a, b, glm::vec2(o.pos.x, o.pos.z), outT);
        return d2 <= r * r;
    }

    bool SegmentIntersectsAabbXZ(const glm::vec3& a, const glm::vec3& b, const DynObstacleState& o, float inflate)
    {
        const float minX = o.pos.x - (o.halfX + inflate);
        const float maxX = o.pos.x + (o.halfX + inflate);
        const float minZ = o.pos.z - (o.halfZ + inflate);
        const float maxZ = o.pos.z + (o.halfZ + inflate);

        float tmin = 0.0f;
        float tmax = 1.0f;
        const float dx = b.x - a.x;
        const float dz = b.z - a.z;

        auto axisSlab = [&](float p0, float d, float mn, float mx) -> bool
        {
            if (std::abs(d) < 1e-6f)
                return p0 >= mn && p0 <= mx;
            float ood = 1.0f / d;
            float t1 = (mn - p0) * ood;
            float t2 = (mx - p0) * ood;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            return tmin <= tmax;
        };

        return axisSlab(a.x, dx, minX, maxX) && axisSlab(a.z, dz, minZ, maxZ);
    }

    bool IsObstacleRelevant(const DynObstacleState& o, uint32_t selfAvoidMask)
    {
        return (o.teamMask & selfAvoidMask) != 0;
    }

    bool SegmentHitsObstacle(const glm::vec3& a,
                            const glm::vec3& b,
                            const DynObstacleState& o,
                            const PathAvoidParamsFFI& params,
                            float* outT)
    {
        if (params.useHeightFilter)
        {
            const float yMid = (a.y + b.y) * 0.5f;
            if (std::abs(yMid - o.pos.y) > std::max(0.0f, params.heightTolerance))
                return false;
        }

        if (o.shapeType == DYNOBS_BOX_AABB)
            return SegmentIntersectsAabbXZ(a, b, o, std::max(0.0f, params.inflate));

        return SegmentIntersectsCircleXZ(a, b, o, std::max(0.0f, params.inflate), outT);
    }

    bool BuildBasePath(ExternNavmeshContext& ctx,
                       const glm::vec3& start,
                       const glm::vec3& end,
                       int flags,
                       int maxPoints,
                       float minEdgeDist,
                       int options,
                       std::vector<glm::vec3>& outCorners,
                       std::vector<NodeInfo>* outInfo)
    {
        std::vector<float> tmp(static_cast<size_t>(maxPoints) * 3);
        std::vector<NodeInfo> info;
        NodeInfo* infoPtr = nullptr;

        if (outInfo)
        {
            info.resize(static_cast<size_t>(maxPoints));
            infoPtr = info.data();
        }

        const int count = RunPathfindInternal(ctx, start, end, flags, maxPoints, minEdgeDist, tmp.data(), infoPtr, options);
        if (count <= 0)
            return false;
        outCorners.clear();
        outCorners.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            outCorners.emplace_back(tmp[i * 3 + 0], tmp[i * 3 + 1], tmp[i * 3 + 2]);

        if (outInfo)
        {
            outInfo->assign(info.begin(), info.begin() + count);
        }
        return true;
    }

    bool IsWalkableSegment(ExternNavmeshContext& ctx, const glm::vec3& from, const glm::vec3& to, int flags)
    {
        if (!EnsureNavQuery(ctx))
            return false;
        dtQueryFilter filter{};
        filter.setIncludeFlags(static_cast<unsigned short>(flags));
        filter.setExcludeFlags(0);
        const float ext[3]{2.0f, 4.0f, 2.0f};
        dtPolyRef fromRef = 0;
        dtPolyRef toRef = 0;
        float fromNearest[3]{};
        float toNearest[3]{};
        const float a[3]{from.x, from.y, from.z};
        const float b[3]{to.x, to.y, to.z};
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(a, ext, &filter, &fromRef, fromNearest)) || fromRef == 0)
            return false;
        if (dtStatusFailed(ctx.navQuery->findNearestPoly(b, ext, &filter, &toRef, toNearest)) || toRef == 0)
            return false;
        float t = 0.0f;
        float hitNormal[3]{};
        dtPolyRef path[16]{};
        int npath = 0;
        const dtStatus st = ctx.navQuery->raycast(fromRef, fromNearest, toNearest, &filter, &t, hitNormal, path, &npath, 16);
        return dtStatusSucceed(st) && t >= 0.999f;
    }
GTANAVVIEWER_API int FindPath(void* navMesh,
                              Vector3 start,
                              Vector3 target,
                              int flags,
                              int maxPoints,
                              float* outPath, int options)
{
    if (!navMesh || !outPath || maxPoints <= 0)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return RunPathfindInternal(*ctx,
                               glm::vec3(start.x, start.y, start.z),
                               glm::vec3(target.x, target.y, target.z),
                               flags,
                               maxPoints,
                               -1.0f,
                               outPath,
                               nullptr,
                               options);
}

GTANAVVIEWER_API int FindPathWithMinEdge(void* navMesh,
                                         Vector3 start,
                                         Vector3 target,
                                         int flags,
                                         int maxPoints,
                                         float minEdge,
                                         float* outPath, int options)
{
    if (!navMesh || !outPath || maxPoints <= 0)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return RunPathfindInternal(*ctx,
                               glm::vec3(start.x, start.y, start.z),
                               glm::vec3(target.x, target.y, target.z),
                               flags,
                               maxPoints,
                               minEdge,
                               outPath,
                               nullptr,
                               options);
}

GTANAVVIEWER_API bool AddOffMeshLink(void* navMesh,
                                     Vector3 start,
                                     Vector3 end,
                                     bool biDir,
                                     bool updateNavMesh)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    OffmeshLink link{};
    link.start = glm::vec3(start.x, start.y, start.z);
    link.end = glm::vec3(end.x, end.y, end.z);
    link.radius = 1.0f;
    link.area = AREA_OFFMESH;
    link.flags = 1;
    link.bidirectional = biDir;
    ctx->offmeshLinks.push_back(link);
    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    ctx->rebuildAll = true;
    if (updateNavMesh)
        return UpdateNavmeshState(*ctx, false);
    return true;
}

GTANAVVIEWER_API int AddOffmeshLinksToNavMeshIsland(void* navMesh,
                                                    const IslandOffmeshLinkParams* params,
                                                    OffmeshLink* outLinks,
                                                    int maxLinks)
{
    if (!navMesh || !params || !outLinks || maxLinks <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    std::vector<OffmeshLink> generated;
    if (!ctx->navData.AddOffmeshLinksToNavMeshIsland(*params, generated))
        return 0;

    const int copyCount = std::min(maxLinks, static_cast<int>(generated.size()));
    for (int i = 0; i < copyCount; ++i)
        outLinks[i] = generated[static_cast<size_t>(i)];
    return copyCount;
}

GTANAVVIEWER_API void ClearOffMeshLinks(void* navMesh, bool updateNavMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->offmeshLinks.clear();
    ctx->navData.ClearOffmeshLinks();
    ctx->rebuildAll = true;
    if (updateNavMesh)
        UpdateNavmeshState(*ctx, false);
}

GTANAVVIEWER_API int GetOffMeshLinks(void* navMesh,
                                     OffMeshLinkInfo* outLinks,
                                     int maxLinks,
                                     int* outLinkCount)
{
    if (outLinkCount)
        *outLinkCount = 0;

    if (!navMesh)
        return 0;

    const auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    const int total = static_cast<int>(ctx->offmeshLinks.size());

    if (!outLinks || maxLinks <= 0)
    {
        if (outLinkCount)
            *outLinkCount = total;
        return 0;
    }

    const int count = std::min(total, maxLinks);
    for (int i = 0; i < count; ++i)
    {
        const auto& src = ctx->offmeshLinks[static_cast<size_t>(i)];
        OffMeshLinkInfo info{};
        info.start = ToVector3(src.start);
        info.end = ToVector3(src.end);
        info.radius = src.radius;
        info.biDir = src.bidirectional;
        info.area = src.area;
        info.flags = src.flags;
        info.userId = src.userId;
        info.ownerTx = src.ownerTx;
        info.ownerTy = src.ownerTy;
        outLinks[i] = info;
    }

    if (outLinkCount)
        *outLinkCount = total;

    return count;
}

GTANAVVIEWER_API int RemoveOffMeshLinksInRadius(void* navMesh,
                                                Vector3 center,
                                                float radius,
                                                bool updateNavMesh)
{
    if (!navMesh || radius < 0.0f)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    const glm::vec3 c(center.x, center.y, center.z);
    const float radiusSq = radius * radius;

    const auto oldSize = ctx->offmeshLinks.size();
    auto& links = ctx->offmeshLinks;
    links.erase(std::remove_if(links.begin(), links.end(), [&](const OffmeshLink& link)
    {
        const float distStart = glm::dot(link.start - c, link.start - c);
        if (distStart <= radiusSq)
            return true;

        const float distEnd = glm::dot(link.end - c, link.end - c);
        return distEnd <= radiusSq;
    }), links.end());

    const int removedCount = static_cast<int>(oldSize - links.size());
    if (removedCount <= 0)
        return 0;

    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    ctx->rebuildAll = true;
    if (updateNavMesh)
        UpdateNavmeshState(*ctx, false);

    return removedCount;
}

GTANAVVIEWER_API bool RemoveOffMeshLinkById(void* navMesh,
                                            unsigned int userId,
                                            bool updateNavMesh)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    auto& links = ctx->offmeshLinks;
    const auto it = std::find_if(links.begin(), links.end(), [&](const OffmeshLink& link)
    {
        return link.userId == userId;
    });

    if (it == links.end())
        return false;

    links.erase(it);
    ctx->navData.SetOffmeshLinks(ctx->offmeshLinks);
    ctx->rebuildAll = true;
    if (updateNavMesh)
        UpdateNavmeshState(*ctx, false);

    return true;
}

GTANAVVIEWER_API bool GenerateAutomaticOffmeshLinks(void* navMesh)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    AutoOffmeshGenerationParamsV2 params = ctx->autoOffmeshParamsV2;
    params.agentRadius = ctx->genSettings.agentRadius;
    params.agentHeight = ctx->genSettings.agentHeight;
    params.maxSlopeDegrees = ctx->genSettings.agentMaxSlope;
    params.outwardOffset = std::max(params.outwardOffset, params.agentRadius + 0.05f);
    params.sweepSideOffset = std::max(0.05f, params.agentRadius * 0.6f);
    params.sweepUp = std::max(0.1f, params.agentHeight * 0.5f);
    
    printf("[ExternC] Gerando offmesh links, genFlags: %d\n", params.genFlags);
    std::vector<OffmeshLink> generated;
    if (!ctx->navData.GenerateAutomaticOffmeshLinksV2(params, generated))
        return false;

    std::vector<OffmeshLink> merged;
    const auto& existing = ctx->navData.GetOffmeshLinks();
    merged.reserve(existing.size() + generated.size());
    for (const auto& link : existing)
        merged.push_back(link);
    merged.insert(merged.end(), generated.begin(), generated.end());

    ctx->navData.SetOffmeshLinks(std::move(merged));
    ctx->offmeshLinks = ctx->navData.GetOffmeshLinks();
    ctx->rebuildAll = true;
    return true;
}

GTANAVVIEWER_API void SetNavMeshBoundingBox(void* navMesh, Vector3 bmin, Vector3 bmax)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->bboxMin = glm::vec3(bmin.x, bmin.y, bmin.z);
    ctx->bboxMax = glm::vec3(bmax.x, bmax.y, bmax.z);
    ctx->hasBoundingBox = true;
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API void RemoveNavMeshBoundingBox(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->hasBoundingBox = false;
    ctx->rebuildAll = true;
}


GTANAVVIEWER_API int UpsertDynamicObstacles(void* navMesh, const DynObstacleDescFFI* obs, int count)
{
    if (!navMesh || !obs || count <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    static bool sLoggedAbi = false;
    if (!sLoggedAbi)
    {
        sLoggedAbi = true;
        std::printf("[ExternC] ABI: sizeof(DynObstacleDescFFI)=%zu sizeof(PathAvoidParamsFFI)=%zu\n",
                    sizeof(DynObstacleDescFFI), sizeof(PathAvoidParamsFFI));
    }

    int upserted = 0;
    for (int i = 0; i < count; ++i)
    {
        const DynObstacleDescFFI& d = obs[i];
        if (d.obstacleId == 0)
            continue;

        const bool exists = ctx->dynObstacles.find(d.obstacleId) != ctx->dynObstacles.end();
        DynObstacleState& st = ctx->dynObstacles[d.obstacleId];
        st.id = d.obstacleId;
        st.teamMask = d.teamMask;
        st.avoidMask = d.avoidMask;
        st.shapeType = (d.shapeType == DYNOBS_BOX_AABB) ? DYNOBS_BOX_AABB : DYNOBS_CYLINDER;
        st.pos = glm::vec3(d.pos[0], d.pos[1], d.pos[2]);
        st.radius = std::max(0.0f, d.radius);
        st.halfX = std::max(0.0f, d.halfX);
        st.halfZ = std::max(0.0f, d.halfZ);
        st.height = std::max(0.0f, d.height);
        if (!exists)
            ctx->dynObstacleIds.push_back(d.obstacleId);
        ++upserted;
    }
    return upserted;
}

GTANAVVIEWER_API int RemoveDynamicObstacles(void* navMesh, const std::uint32_t* obstacleIds, int count)
{
    if (!navMesh || !obstacleIds || count <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    int removed = 0;
    for (int i = 0; i < count; ++i)
    {
        if (ctx->dynObstacles.erase(obstacleIds[i]) > 0)
            ++removed;
    }

    if (removed > 0)
    {
        auto& ids = ctx->dynObstacleIds;
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](uint32_t id)
        {
            return ctx->dynObstacles.find(id) == ctx->dynObstacles.end();
        }), ids.end());
    }

    return removed;
}

GTANAVVIEWER_API void ClearDynamicObstacles(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->dynObstacles.clear();
    ctx->dynObstacleIds.clear();
}

GTANAVVIEWER_API int FindPathAvoidingDynamicObstacles(
    void* navMesh,
    Vector3 start,
    Vector3 target,
    int flags,
    int maxPoints,
    float minEdgeDist,
    int options,
    const PathAvoidParamsFFI* avoidParams,
    std::uint32_t selfAvoidMask,
    std::uint32_t ignoreObstacleId,
    float* outPathXYZ,
    NodeInfo* outNodeInfo)
{
    if (!navMesh || !outPathXYZ)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);

    PathAvoidParamsFFI params = avoidParams ? *avoidParams : GetDefaultAvoidParams();

    glm::vec3 curStart(start.x, start.y, start.z);
    glm::vec3 finalTarget(target.x, target.y, target.z);

    std::vector<float> path(maxPoints * 3);
    std::vector<NodeInfo> nodeInfo(maxPoints);

    int pathCount = 0;

    for (int iter = 0; iter < params.maxFixIterations; ++iter)
    {
        pathCount = RunPathfindInternal(
            *ctx,
            curStart,
            finalTarget,
            flags,
            maxPoints,
            minEdgeDist,
            path.data(),
            nodeInfo.data(),
            options);

        if (pathCount == 1)
        {
            outPathXYZ[0] = start.x;
            outPathXYZ[1] = start.y;
            outPathXYZ[2] = start.z;

            outPathXYZ[3] = target.x;
            outPathXYZ[4] = target.y;
            outPathXYZ[5] = target.z;

            return 2;
        }

        bool foundObstacle = false;

        for (int i = 0; i + 1 < pathCount; ++i)
        {
            glm::vec3 a(path[i*3+0], path[i*3+1], path[i*3+2]);
            glm::vec3 b(path[(i+1)*3+0], path[(i+1)*3+1], path[(i+1)*3+2]);

            const DynObstacleState* blocker = nullptr;
            float bestScore = std::numeric_limits<float>::max();
            int checked = 0;

            for (uint32_t id : ctx->dynObstacleIds)
            {
                if (id == ignoreObstacleId)
                    continue;

                auto it = ctx->dynObstacles.find(id);
                if (it == ctx->dynObstacles.end())
                    continue;

                const DynObstacleState& o = it->second;

                if (!IsObstacleRelevant(o, selfAvoidMask))
                    continue;

                if (checked++ >= params.maxObstaclesToCheck)
                    break;

                float hitT = 0.0f;

                if (!SegmentHitsObstacle(a, b, o, params, &hitT))
                    continue;

                if (hitT < bestScore)
                {
                    bestScore = hitT;
                    blocker = &o;
                }
            }

            if (!blocker)
                continue;

            foundObstacle = true;

            // gerar candidato lateral
            glm::vec2 dir(b.x - a.x, b.z - a.z);
            if (glm::dot(dir, dir) < 1e-6f)
                dir = glm::vec2(1,0);

            dir = glm::normalize(dir);

            glm::vec2 left(-dir.y, dir.x);

            float step = blocker->radius + params.inflate + params.detourSideStep;

            glm::vec3 cand1(
                blocker->pos.x + left.x * step,
                a.y,
                blocker->pos.z + left.y * step);

            glm::vec3 cand2(
                blocker->pos.x - left.x * step,
                a.y,
                blocker->pos.z - left.y * step);

            glm::vec3 candidate = cand1;

            if (!IsWalkableSegment(*ctx, a, cand1, flags))
                candidate = cand2;

            // próximo path começa no candidato
            curStart = candidate;

            break;
        }

        if (!foundObstacle)
            break;
    }

    if (pathCount <= 0)
        return 0;

    int writeCount = std::min(maxPoints, pathCount);

    for (int i = 0; i < writeCount; ++i)
    {
        outPathXYZ[i*3+0] = path[i*3+0];
        outPathXYZ[i*3+1] = path[i*3+1];
        outPathXYZ[i*3+2] = path[i*3+2];

        if (outNodeInfo)
            outNodeInfo[i] = nodeInfo[i];
    }

    return writeCount;
}

GTANAVVIEWER_API int UpsertSimAgents(void* navMesh, const SimAgentDescFFI* agents, int count)
{
    if (!navMesh || !agents || count <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    int upserted = 0;

    static bool sLoggedAbi = false;
    if (!sLoggedAbi)
    {
        sLoggedAbi = true;
        std::printf("[ExternC] ABI: sizeof(SimAgentDescFFI)=%zu sizeof(SimParamsFFI)=%zu\n",
                    sizeof(SimAgentDescFFI), sizeof(SimParamsFFI));
    }

    const SimParamsFFI params = ctx->hasLastSimParams ? ctx->lastSimParams : SimParamsFFI{};
    const float anchorHeadingAlpha = std::clamp(params.anchorHeadingAlpha, 0.0f, 1.0f);
    const float anchorVelAlpha = std::clamp(params.anchorVelAlpha, 0.0f, 1.0f);

    for (int i = 0; i < count; ++i)
    {
        const SimAgentDescFFI& d = agents[i];
        SimAgentState& st = ctx->simAgents[d.agentId];
        const bool isNew = st.id == 0;
        st.id = d.agentId;

        // Sempre atualiza descritores.
        st.teamMask = d.teamMask;
        st.avoidMask = d.avoidMask;
        st.flags = d.flags;
        st.shape = (d.shapeType == SHAPE_BOX) ? SHAPE_BOX : SHAPE_CYLINDER;
        st.radius = std::max(0.05f, d.radius);
        st.halfX = std::max(0.05f, d.halfX);
        st.halfZ = std::max(0.05f, d.halfZ);
        st.height = std::max(0.1f, d.height);

        const glm::vec3 inPos(d.pos[0], d.pos[1], d.pos[2]);
        const glm::vec3 inVel(d.vel[0], d.vel[1], d.vel[2]);
        const float inHeading = WrapAngleDeg(d.headingDeg);

        const bool wantsTeleport = (d.flags & AGENT_TELEPORT) != 0;
        const bool wantsAnchor = !wantsTeleport && (d.flags & AGENT_ANCHOR) != 0;
        const bool anchorHeading = wantsAnchor && ((d.flags & AGENT_ANCHOR_HEADING) != 0);
        const bool anchorVelocity = wantsAnchor && ((d.flags & AGENT_ANCHOR_VELOCITY) != 0);

        auto teleportReset = [&]()
        {
            st.pos = inPos;
            st.headingDeg = inHeading;
            st.lastVel = inVel;
            st.verticalVel = 0.0f;
            st.moveSurfaceFailCount = 0;
            ResetAgentPathState(st);
            if (!RefreshAgentNearestPoly(*ctx, st))
                st.currentRef = 0;
        };

        if (isNew)
        {
            teleportReset();
            st.lastVel = glm::vec3(0.0f);
            if (!RefreshAgentNearestPoly(*ctx, st))
                st.currentRef = 0;
            ctx->simAgentIds.push_back(d.agentId);
#ifdef GTANAVVIEWER_SIM_DEBUG_UPSERT
            std::printf("[Sim] Upsert NEW id=%u\n", d.agentId);
#endif
        }
        else if (wantsTeleport)
        {
            teleportReset();
#ifdef GTANAVVIEWER_SIM_DEBUG_UPSERT
            std::printf("[Sim] Upsert TELEPORT id=%u\n", d.agentId);
#endif
        }
        else if (wantsAnchor)
        {
            const glm::vec3 oldPos = st.pos;
            st.pos = inPos;

            const float yawDelta = std::abs(WrapAngleDeg(inHeading - st.headingDeg));
            const bool headingSnap = params.anchorMaxSnapYawDeg > 0.0f && yawDelta > params.anchorMaxSnapYawDeg;
            if (anchorHeading)
            {
                st.headingDeg = headingSnap ? inHeading : LerpAngleDeg(st.headingDeg, inHeading, anchorHeadingAlpha);
            }

            if (anchorVelocity)
            {
                st.lastVel = st.lastVel + (inVel - st.lastVel) * anchorVelAlpha;
            }
            else
            {
                ClampAgentHorizontalSpeed(params, st);
            }

            const float anchorDist = glm::length(glm::vec2(inPos.x - oldPos.x, inPos.z - oldPos.z));
            const bool shouldSnapTeleport = params.anchorMaxSnapDist > 0.0f && anchorDist > params.anchorMaxSnapDist;
            if (shouldSnapTeleport)
            {
                teleportReset();
            }
            else
            {
                if (!RefreshAgentNearestPoly(*ctx, st))
                {
                    st.currentRef = 0;
                    ResetAgentPathState(st);
                }
            }
#ifdef GTANAVVIEWER_SIM_DEBUG_UPSERT
            std::printf("[Sim] Upsert ANCHOR id=%u\n", d.agentId);
#endif
        }
#ifdef GTANAVVIEWER_SIM_DEBUG_UPSERT
        else
        {
            std::printf("[Sim] Upsert PARAMS_ONLY id=%u\n", d.agentId);
        }
#endif

        ++upserted;
    }
    return upserted;
}

GTANAVVIEWER_API int RemoveSimAgents(void* navMesh, const uint32_t* agentIds, int count)
{
    if (!navMesh || !agentIds || count <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    int removed = 0;
    for (int i = 0; i < count; ++i)
    {
        auto it = ctx->simAgents.find(agentIds[i]);
        if (it == ctx->simAgents.end())
            continue;
        ctx->simAgents.erase(it);
        ++removed;
    }

    if (removed > 0)
    {
        auto& ids = ctx->simAgentIds;
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](uint32_t id)
        {
            return ctx->simAgents.find(id) == ctx->simAgents.end();
        }), ids.end());
    }
    return removed;
}

GTANAVVIEWER_API void ClearSimAgents(void* navMesh)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->simAgents.clear();
    ctx->simAgentIds.clear();
}

GTANAVVIEWER_API int ComputeAgentPathEx(void* navMesh,
                                        uint32_t agentId,
                                        Vector3 start,
                                        Vector3 target,
                                        int flags,
                                        int maxCorners,
                                        float minEdgeDist,
                                        int options,
                                        std::uint32_t pathModeFlags)
{
    if (!navMesh || maxCorners <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    auto it = ctx->simAgents.find(agentId);
    if (it == ctx->simAgents.end())
        return 0;

    SimAgentState& agent = it->second;
    const bool hardReset = (pathModeFlags & SIM_PATH_HARD_RESET) != 0;
    const bool keepCornerIfValid = (pathModeFlags & SIM_PATH_KEEP_CORNER_INDEX_IF_VALID) != 0;
    const bool findBySegment = (pathModeFlags & SIM_PATH_FIND_CORNER_BY_SEGMENT) != 0;

    const glm::vec3 requestedStart(start.x, start.y, start.z);
    const glm::vec3 usedStart = hardReset ? requestedStart : agent.pos;

#ifdef GTANAVVIEWER_SIM_DEBUG_PATH
    std::printf("[Sim] ComputeAgentPath id=%u mode=0x%08X hard=%d start=(%.3f,%.3f,%.3f) usedStart=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f)\n",
                agentId,
                pathModeFlags,
                hardReset ? 1 : 0,
                requestedStart.x, requestedStart.y, requestedStart.z,
                usedStart.x, usedStart.y, usedStart.z,
                target.x, target.y, target.z);
#endif

    std::vector<float> corners;
    std::vector<unsigned char> cornerFlags;
    std::vector<dtPolyRef> pathPolys;
    dtPolyRef startRef = 0;
    float startNearest[3]{};
    if (!ComputePathData(*ctx,
                         usedStart,
                         glm::vec3(target.x, target.y, target.z),
                         flags,
                         maxCorners,
                         minEdgeDist,
                         options,
                         corners,
                         cornerFlags,
                         pathPolys,
                         &startRef,
                         startNearest))
    {
        return 0;
    }

    const int oldCornerIndex = agent.cornerIndex;
    const int oldCornerCount = agent.cornerCount;

    agent.cornersXYZ = std::move(corners);
    agent.cornerFlags = std::move(cornerFlags);
    agent.cornerCount = static_cast<int>(agent.cornerFlags.size());
    agent.pathPolys = std::move(pathPolys);
    agent.currentRef = startRef;

    if (hardReset)
    {
        agent.cornerIndex = 0;
        agent.pos = glm::vec3(startNearest[0], startNearest[1], startNearest[2]);
        agent.lastVel = glm::vec3(0.0f);
        agent.verticalVel = 0.0f;
        agent.moveSurfaceFailCount = 0;
        if (agent.cornerCount <= 0)
            ResetAgentPathState(agent);
        else if (!RefreshAgentNearestPoly(*ctx, agent, flags))
            agent.currentRef = startRef;
#ifdef GTANAVVIEWER_SIM_DEBUG_PATH
        std::printf("[Sim] ComputeAgentPath hard-reset id=%u corners=%d idx=%d\n", agentId, agent.cornerCount, agent.cornerIndex);
#endif
        return agent.cornerCount;
    }

    int selectedCornerIndex = 0;
    bool usedFallbackCorner0 = false;
    if (agent.cornerCount > 0)
    {
        if (keepCornerIfValid && oldCornerCount > 0 && oldCornerIndex >= 0 && oldCornerIndex < agent.cornerCount)
        {
            selectedCornerIndex = oldCornerIndex;
        }
        else
        {
            selectedCornerIndex = findBySegment
                ? FindBestCornerIndexBySegment(agent.pos, agent.cornersXYZ, agent.cornerCount, kSoftRepathMaxSnapDist)
                : FindBestCornerIndexByCorner(agent.pos, agent.cornersXYZ, agent.cornerCount, kSoftRepathMaxSnapDist);

            const glm::vec3 selectedCorner = CornerAt(agent.cornersXYZ, selectedCornerIndex);
            usedFallbackCorner0 = (selectedCornerIndex == 0) &&
                                  (Distance2DSq(agent.pos, selectedCorner) > kSoftRepathMaxSnapDist * kSoftRepathMaxSnapDist);
        }

        if (oldCornerCount > 0)
            selectedCornerIndex = std::max(selectedCornerIndex, std::max(0, oldCornerIndex - 2));

        agent.cornerIndex = std::clamp(selectedCornerIndex, 0, agent.cornerCount - 1);
    }
    else
    {
        agent.cornerIndex = 0;
    }

#ifdef GTANAVVIEWER_SIM_DEBUG_PATH
    std::printf("[Sim] ComputeAgentPath soft-repath id=%u corners=%d idx=%d fallback0=%d\n",
                agentId,
                agent.cornerCount,
                agent.cornerIndex,
                usedFallbackCorner0 ? 1 : 0);
#endif

    return agent.cornerCount;
}

GTANAVVIEWER_API int ComputeAgentPath(void* navMesh,
                                      uint32_t agentId,
                                      Vector3 start,
                                      Vector3 target,
                                      int flags,
                                      int maxCorners,
                                      float minEdgeDist,
                                      int options)
{
    return ComputeAgentPathEx(navMesh,
                              agentId,
                              start,
                              target,
                              flags,
                              maxCorners,
                              minEdgeDist,
                              options,
                              SIM_PATH_SOFT_REPATH);
}

GTANAVVIEWER_API void EnableHeightSampling(void* navMesh, bool enabled)
{
    if (!navMesh)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->heightSampler.enabled = enabled;
}

GTANAVVIEWER_API bool BuildHeightSamplerForCurrentGeometry(void* navMesh, int samplesPerTile, bool storeTwoLayers)
{
    if (!navMesh)
        return false;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->heightSampler.samplesPerTile = std::max(8, samplesPerTile);
    ctx->heightSampler.storeTwoLayers = storeTwoLayers;
    ctx->heightSampler.built = true;
    return true;
}

static int SimulateAgentsInternal(ExternNavmeshContext& ctx,
                                  const uint32_t* agentIds,
                                  int agentCount,
                                  float dt,
                                  int maxSimulationFrames,
                                  const SimParamsFFI* params,
                                  float* outPosXYZ,
                                  float* outHeadingDeg,
                                  float* outVelXYZ,
                                  uint8_t* outFrameFlags,
                                  float* outEulerRPYDeg,
                                  SimEventFFI* outEvents,
                                  int maxEvents)
{
    if (!params || !outPosXYZ || !outHeadingDeg || !outVelXYZ || !outFrameFlags || !agentIds || agentCount <= 0 || maxSimulationFrames <= 0 || dt <= 0.0f)
        return 0;
    if (!EnsureNavQuery(ctx))
        return 0;

    ctx.lastSimParams = *params;
    ctx.hasLastSimParams = true;

    std::vector<SimAgentState*> agents;
    agents.reserve(static_cast<size_t>(agentCount));
    for (int i = 0; i < agentCount; ++i)
    {
        auto it = ctx.simAgents.find(agentIds[i]);
        if (it == ctx.simAgents.end())
            continue;
        agents.push_back(&it->second);
    }
    if (agents.empty())
        return 0;

    int eventCount = 0;
    for (int frame = 0; frame < maxSimulationFrames; ++frame)
    {
        for (size_t ai = 0; ai < agents.size(); ++ai)
        {
            SimAgentState& agent = *agents[ai];
            const size_t basePos = (ai * static_cast<size_t>(maxSimulationFrames) + static_cast<size_t>(frame)) * 3;
            const size_t baseScalar = ai * static_cast<size_t>(maxSimulationFrames) + static_cast<size_t>(frame);
            uint8_t frameFlags = 0;

            if ((agent.flags & AGENT_ENABLED) == 0 || agent.cornerCount <= 0 || agent.cornerIndex >= agent.cornerCount)
            {
                frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
                outPosXYZ[basePos + 0] = agent.pos.x;
                outPosXYZ[basePos + 1] = agent.pos.y;
                outPosXYZ[basePos + 2] = agent.pos.z;
                outHeadingDeg[baseScalar] = agent.headingDeg;
                outVelXYZ[basePos + 0] = 0.0f;
                outVelXYZ[basePos + 1] = 0.0f;
                outVelXYZ[basePos + 2] = 0.0f;
                outFrameFlags[baseScalar] = frameFlags;
                if ((agent.flags & AGENT_VEHICLE) == 0 || agent.shape != SHAPE_BOX)
                {
                    agent.rollDeg = 0.0f;
                    agent.pitchDeg = 0.0f;
                    agent.eulerRPYDeg = glm::vec3(0.0f, 0.0f, agent.headingDeg);
                }
                if (outEulerRPYDeg)
                {
                    const size_t baseEuler = baseScalar * 3;
                    const bool isVehicleBoxOut = ((agent.flags & AGENT_VEHICLE) != 0) && agent.shape == SHAPE_BOX;
                    outEulerRPYDeg[baseEuler + 0] = isVehicleBoxOut ? agent.rollDeg : 0.0f;
                    outEulerRPYDeg[baseEuler + 1] = isVehicleBoxOut ? agent.pitchDeg : 0.0f;
                    outEulerRPYDeg[baseEuler + 2] = agent.headingDeg;
                }
                continue;
            }

            const bool isVehicleBox = ((agent.flags & AGENT_VEHICLE) != 0) && agent.shape == SHAPE_BOX;
            auto emitOffmeshFrame = [&]()
            {
                agent.offT += dt;
                float u = (agent.offDuration > 1e-4f) ? (agent.offT / agent.offDuration) : 1.0f;
                u = std::clamp(u, 0.0f, 1.0f);

                glm::vec3 p = glm::mix(agent.offStart, agent.offEnd, u);
                if (agent.offArcHeight > 0.0f)
                {
                    const float arc = 4.0f * u * (1.0f - u);
                    p.y += arc * agent.offArcHeight;
                }

                glm::vec3 d = agent.offEnd - agent.pos;
                d.y = 0.0f;
                if (glm::dot(d, d) > 1e-6f)
                {
                    d = glm::normalize(d);
                    const float targetHeading = std::atan2(d.x, d.z) * 180.0f / 3.1415926535f;
                    float deltaYaw = WrapAngleDeg(targetHeading - agent.headingDeg);
                    const float maxTurn = std::max(0.0f, params->agentTurnSpeedDeg) * dt;
                    deltaYaw = std::clamp(deltaYaw, -maxTurn, maxTurn);
                    if (std::isfinite(deltaYaw))
                        agent.headingDeg = WrapAngleDeg(agent.headingDeg + deltaYaw);
                }

                if (dt > 1e-6f)
                    agent.lastVel = (p - agent.pos) / dt;
                else
                    agent.lastVel = glm::vec3(0.0f);
                agent.pos = p;

                frameFlags |= SIM_FRAMEFLAG_JUMP;
                frameFlags |= SIM_FRAMEFLAG_OFFMESH_TRAVERSAL;

                if (u >= 1.0f)
                {
                    agent.inOffmesh = false;
                    agent.pos = agent.offEnd;
                    agent.verticalVel = 0.0f;
                    agent.offT = 0.0f;
                    agent.offDuration = 0.0f;
                    agent.offArcHeight = 0.0f;
                    agent.offType = 0;
                    agent.offmeshStartCornerIndex = -1;
                    agent.cornerIndex += 2;
                    if (agent.cornerIndex >= agent.cornerCount)
                    {
                        frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
                    }
                    else if (!RefreshAgentNearestPoly(ctx, agent))
                    {
                        agent.currentRef = 0;
                        frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
                    }
                }

                outPosXYZ[basePos + 0] = agent.pos.x;
                outPosXYZ[basePos + 1] = agent.pos.y;
                outPosXYZ[basePos + 2] = agent.pos.z;
                outHeadingDeg[baseScalar] = agent.headingDeg;
                outVelXYZ[basePos + 0] = agent.lastVel.x;
                outVelXYZ[basePos + 1] = agent.lastVel.y;
                outVelXYZ[basePos + 2] = agent.lastVel.z;
                outFrameFlags[baseScalar] = frameFlags;
                if (!isVehicleBox)
                {
                    agent.rollDeg = 0.0f;
                    agent.pitchDeg = 0.0f;
                    agent.eulerRPYDeg = glm::vec3(0.0f, 0.0f, agent.headingDeg);
                }
                if (outEulerRPYDeg)
                {
                    const size_t baseEuler = baseScalar * 3;
                    outEulerRPYDeg[baseEuler + 0] = isVehicleBox ? agent.rollDeg : 0.0f;
                    outEulerRPYDeg[baseEuler + 1] = isVehicleBox ? agent.pitchDeg : 0.0f;
                    outEulerRPYDeg[baseEuler + 2] = agent.headingDeg;
                }
            };

            if (agent.inOffmesh)
            {
                emitOffmeshFrame();
                continue;
            }

            glm::vec3 target(agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 0],
                             agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 1],
                             agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 2]);
            glm::vec3 toTarget = target - agent.pos;
            toTarget.y = 0.0f;
            const float dist = glm::length(toTarget);
            if (dist <= std::max(params->reachRadius, 0.1f))
            {
                const bool wasOffmesh = (agent.cornerFlags[static_cast<size_t>(agent.cornerIndex)] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
                if (wasOffmesh && agent.cornerIndex + 1 < agent.cornerCount)
                {
                    const glm::vec3 end(agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex + 1) * 3 + 0],
                                        agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex + 1) * 3 + 1],
                                        agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex + 1) * 3 + 2]);
                    agent.inOffmesh = true;
                    agent.offStart = agent.pos;
                    agent.offEnd = end;
                    agent.offT = 0.0f;
                    const float distToEnd = glm::length(agent.offEnd - agent.offStart);
                    const float baseSpeed = std::max((agent.flags & AGENT_VEHICLE) != 0 ? params->maxSpeedForward : params->agentSpeed, 0.1f);
                    agent.offDuration = std::clamp(distToEnd / baseSpeed, 0.15f, 1.25f);
                    if (agent.offDuration <= 1e-4f)
                        agent.offDuration = 0.2f;
                    agent.offType = 2;
                    agent.offArcHeight = ((agent.flags & AGENT_VEHICLE) != 0) ? 0.0f : std::clamp(distToEnd * 0.15f, 0.0f, 1.2f);
                    agent.offmeshStartCornerIndex = agent.cornerIndex;

                    eventCount = AppendJumpEvent(agent, frame, target, end, outEvents, maxEvents, eventCount, params->agentSpeed, agent.offDuration);
                    emitOffmeshFrame();
                    continue;
                }
                if (wasOffmesh)
                {
                    frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
                    agent.cornerIndex = agent.cornerCount;
                }
                else
                {
                    ++agent.cornerIndex;
                    if (agent.cornerIndex >= agent.cornerCount)
                        frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
                }
            }

            glm::vec3 desiredDir(0.0f);
            if (agent.cornerIndex < agent.cornerCount)
            {
                target = glm::vec3(agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 0],
                                   agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 1],
                                   agent.cornersXYZ[static_cast<size_t>(agent.cornerIndex) * 3 + 2]);
                toTarget = target - agent.pos;
                toTarget.y = 0.0f;
                const float d = glm::length(toTarget);
                if (d > 1e-4f)
                    desiredDir = toTarget / d;
            }

            std::vector<const SimAgentState*> neighbors;
            neighbors.reserve(16);
            const float avoidRangeSq = params->avoidRange * params->avoidRange;
            for (size_t oi = 0; oi < agents.size(); ++oi)
            {
                if (oi == ai)
                    continue;
                const SimAgentState& other = *agents[oi];
                glm::vec3 diff = other.pos - agent.pos;
                diff.y = 0.0f;
                if (glm::dot(diff, diff) <= avoidRangeSq)
                    neighbors.push_back(&other);
            }

            glm::vec3 avoid = ComputeAvoidanceForce(agent, neighbors, params->avoidRange, params->avoidWeight);
            glm::vec3 moveDir(0.0f);
            if (isVehicleBox)
            {
                float targetHeading = agent.headingDeg;
                if (glm::dot(desiredDir, desiredDir) > 1e-6f)
                    targetHeading = std::atan2(desiredDir.x, desiredDir.z) * 180.0f / 3.1415926535f;
                float deltaYaw = WrapAngleDeg(targetHeading - agent.headingDeg);
                const float maxTurn = std::max(0.0f, params->agentTurnSpeedDeg) * dt;
                deltaYaw = std::clamp(deltaYaw, -maxTurn, maxTurn);
                if (std::isfinite(deltaYaw))
                    agent.headingDeg = WrapAngleDeg(agent.headingDeg + deltaYaw);

                const float yawRad = glm::radians(agent.headingDeg);
                moveDir = glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad));
            }
            else
            {
                moveDir = desiredDir + avoid;
                moveDir.y = 0.0f;
                const float moveLen = glm::length(moveDir);
                if (moveLen > 1e-4f)
                    moveDir /= moveLen;
            }

            float speed = std::max(0.0f, params->agentSpeed);
            if ((agent.flags & AGENT_VEHICLE) != 0)
                speed = std::max(0.0f, params->maxSpeedForward);
            glm::vec3 desiredVel = moveDir * speed;
            glm::vec3 velDelta = desiredVel - agent.lastVel;
            const float maxDelta = std::max(0.0f, params->agentAccel) * dt;
            const float velDeltaLen = glm::length(velDelta);
            if (velDeltaLen > maxDelta && maxDelta > 0.0f)
                velDelta = velDelta / velDeltaLen * maxDelta;
            agent.lastVel += velDelta;
            if (isVehicleBox)
                agent.lastVel.y = 0.0f;

            glm::vec3 candidate = agent.pos + agent.lastVel * dt;
            float startPos[3] = { agent.pos.x, agent.pos.y, agent.pos.z };
            float endPos[3] = { candidate.x, candidate.y, candidate.z };
            float result[3] = { agent.pos.x, agent.pos.y, agent.pos.z };
            dtPolyRef visited[32]{};
            int visitedCount = 0;
            dtQueryFilter filter{};
            filter.setIncludeFlags(0xFFFF);
            if (agent.currentRef != 0)
            {
                if (dtStatusSucceed(ctx.navQuery->moveAlongSurface(agent.currentRef, startPos, endPos, &filter, result, visited, &visitedCount, 32)))
                {
                    agent.moveSurfaceFailCount = 0;
                    if (visitedCount > 0)
                        agent.currentRef = visited[visitedCount - 1];
                    candidate = glm::vec3(result[0], result[1], result[2]);
                }
                else
                {
                    ++agent.moveSurfaceFailCount;
                }
            }
            else
            {
                ++agent.moveSurfaceFailCount;
            }

            if (agent.currentRef == 0 || agent.moveSurfaceFailCount >= 3)
            {
                if (RefreshAgentNearestPoly(ctx, agent))
                    agent.moveSurfaceFailCount = 0;
                else
                    frameFlags |= SIM_FRAMEFLAG_NEEDS_REPATH;
            }

            float h = candidate.y;
            if (ctx.heightSampler.enabled)
                h = SampleAgentHeight(ctx, agent, candidate);
            if (isVehicleBox)
            {
                const bool fitOk = FitVehicleToGround(ctx, agent, *params, dt, frameFlags);
                if (!fitOk)
                {
                    if (params->gravity > 0.0f)
                    {
                        agent.verticalVel = std::max(agent.verticalVel - params->gravity * dt, -std::max(params->maxFallSpeed, 0.0f));
                        const float targetY = h;
                        agent.pos.y += agent.verticalVel * dt;
                        if (agent.pos.y < targetY)
                        {
                            agent.pos.y = targetY;
                            agent.verticalVel = 0.0f;
                        }
                    }
                }
            }
            else if (params->gravity > 0.0f)
            {
                agent.verticalVel = std::max(agent.verticalVel - params->gravity * dt, -std::max(params->maxFallSpeed, 0.0f));
                const float targetY = h;
                agent.pos.y += agent.verticalVel * dt;
                if (agent.pos.y < targetY)
                {
                    agent.pos.y = targetY;
                    agent.verticalVel = 0.0f;
                }
            }
            else
            {
                agent.pos.y = h;
            }

            const float moved = glm::length(glm::vec2(candidate.x - agent.pos.x, candidate.z - agent.pos.z));
            agent.pos.x = candidate.x;
            agent.pos.z = candidate.z;
            if (moved < 0.001f)
                frameFlags |= SIM_FRAMEFLAG_STUCK;

            if (!isVehicleBox)
            {
                const float targetHeading = std::atan2(moveDir.x, moveDir.z) * 180.0f / 3.1415926535f;
                float deltaYaw = WrapAngleDeg(targetHeading - agent.headingDeg);
                const float maxTurn = std::max(0.0f, params->agentTurnSpeedDeg) * dt;
                deltaYaw = std::clamp(deltaYaw, -maxTurn, maxTurn);
                if (std::isfinite(deltaYaw))
                    agent.headingDeg = WrapAngleDeg(agent.headingDeg + deltaYaw);
            }

            if (agent.cornerIndex < agent.cornerCount)
            {
                const bool offmeshSoon = (agent.cornerFlags[static_cast<size_t>(agent.cornerIndex)] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
                if (offmeshSoon)
                    frameFlags |= SIM_FRAMEFLAG_OFFMESH_SOON;
            }

            outPosXYZ[basePos + 0] = agent.pos.x;
            outPosXYZ[basePos + 1] = agent.pos.y;
            outPosXYZ[basePos + 2] = agent.pos.z;
            outHeadingDeg[baseScalar] = agent.headingDeg;
            outVelXYZ[basePos + 0] = agent.lastVel.x;
            outVelXYZ[basePos + 1] = agent.lastVel.y;
            outVelXYZ[basePos + 2] = agent.lastVel.z;
            outFrameFlags[baseScalar] = frameFlags;
            if (!isVehicleBox)
            {
                agent.rollDeg = 0.0f;
                agent.pitchDeg = 0.0f;
                agent.eulerRPYDeg = glm::vec3(0.0f, 0.0f, agent.headingDeg);
            }
            if (outEulerRPYDeg)
            {
                const size_t baseEuler = baseScalar * 3;
                outEulerRPYDeg[baseEuler + 0] = isVehicleBox ? agent.rollDeg : 0.0f;
                outEulerRPYDeg[baseEuler + 1] = isVehicleBox ? agent.pitchDeg : 0.0f;
                outEulerRPYDeg[baseEuler + 2] = agent.headingDeg;
            }
        }
    }

    return maxSimulationFrames;
}

GTANAVVIEWER_API int SimulateAgentFrames(void* navMesh,
                                         uint32_t agentId,
                                         float dt,
                                         int maxSimulationFrames,
                                         const SimParamsFFI* params,
                                         float* outPosXYZ,
                                         float* outHeadingDeg,
                                         float* outVelXYZ,
                                         uint8_t* outFrameFlags,
                                         float* outEulerRPYDeg,
                                         SimEventFFI* outEvents,
                                         int maxEvents)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    const uint32_t ids[1] = { agentId };
    return SimulateAgentsInternal(*ctx,
                                  ids,
                                  1,
                                  dt,
                                  maxSimulationFrames,
                                  params,
                                  outPosXYZ,
                                  outHeadingDeg,
                                  outVelXYZ,
                                  outFrameFlags,
                                  outEulerRPYDeg,
                                  outEvents,
                                  maxEvents);
}

GTANAVVIEWER_API int SimulateAgentsFramesBatch(void* navMesh,
                                               const uint32_t* agentIds,
                                               int agentCount,
                                               float dt,
                                               int maxSimulationFrames,
                                               const SimParamsFFI* params,
                                               float* outPosXYZ,
                                               float* outHeadingDeg,
                                               float* outVelXYZ,
                                               uint8_t* outFrameFlags,
                                               float* outEulerRPYDeg,
                                               SimEventFFI* outEvents,
                                               int maxEvents)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return SimulateAgentsInternal(*ctx,
                                  agentIds,
                                  agentCount,
                                  dt,
                                  maxSimulationFrames,
                                  params,
                                  outPosXYZ,
                                  outHeadingDeg,
                                  outVelXYZ,
                                  outFrameFlags,
                                  outEulerRPYDeg,
                                  outEvents,
                                  maxEvents);
}
