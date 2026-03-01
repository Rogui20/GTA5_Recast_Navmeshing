#include "ExternC.h"
#include "NavMesh_TileCacheDB.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cfloat>
#include <cstdio>
#include <cmath>
#include <cstring>
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

    struct ExternNavmeshContext
    {
        NavmeshGenerationSettings genSettings{};
        std::vector<GeometryInstance> geometries;
        std::vector<OffmeshLink> offmeshLinks;
        AutoOffmeshGenerationParams autoOffmeshParams{};
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
        std::unordered_map<uint64_t, uint32_t> residentStamp;
        std::unordered_set<uint64_t> residentTiles;
        uint32_t stampCounter = 0;
        std::unordered_map<uint64_t, TileDbIndexEntry> dbIndexCache;
        bool dbIndexLoaded = false;
        std::filesystem::file_time_type dbMTime{};
        std::unordered_map<std::uint32_t, SimAgentState> simAgents;
        std::vector<std::uint32_t> simAgentIds;
        HeightSampler heightSampler;
        SimParamsFFI lastSimParams{};
        bool hasLastSimParams = false;
    };

    std::filesystem::path GetSessionCachePath(const ExternNavmeshContext& ctx);
    bool EnsureNavQuery(ExternNavmeshContext& ctx);
    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild);

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

    bool RebuildDirtyTiles(ExternNavmeshContext& ctx)
    {
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

    bool UpdateNavmeshState(ExternNavmeshContext& ctx, bool forceFullBuild)
    {
        if (ctx.streamingEnabled)
        {
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
    ctx->rebuildAll = true;
}

GTANAVVIEWER_API void SetAutoOffMeshGenerationParams(void* navMesh, const AutoOffmeshGenerationParams* params)
{
    if (!navMesh || !params)
        return;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    ctx->autoOffmeshParams = *params;
    ctx->autoOffmeshParams.agentRadius = ctx->genSettings.agentRadius;
    ctx->autoOffmeshParams.agentHeight = ctx->genSettings.agentHeight;
    ctx->autoOffmeshParams.maxSlopeDegrees = ctx->genSettings.agentMaxSlope;
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
    ctx->geometries.clear();
    ctx->rebuildAll = true;
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

    const auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    if (ctx->geometries.empty())
        return false;

    std::filesystem::path outPath(outputObjPath);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path());

    std::ofstream out(outPath);
    if (!out.is_open())
        return false;

    out << "# Merged navmesh geometries\n";

    std::size_t globalVertexOffset = 1;
    for (const auto& geom : ctx->geometries)
    {
        if (!geom.source.Valid())
            continue;

        const glm::mat3 rotation = GetRotationMatrix(geom.rotation);

        out << "\no geometry_" << geom.id << "\n";
        out << "# position " << geom.position.x << " " << geom.position.y << " " << geom.position.z << "\n";
        out << "# rotationDeg " << geom.rotation.x << " " << geom.rotation.y << " " << geom.rotation.z << "\n";

        for (const auto& v : geom.source.vertices)
        {
            const glm::vec3 transformed = rotation * v + geom.position;
            out << "v " << transformed.x << " " << transformed.y << " " << transformed.z << "\n";
        }

        for (size_t i = 0; i + 2 < geom.source.indices.size(); i += 3)
        {
            const std::size_t i0 = globalVertexOffset + geom.source.indices[i];
            const std::size_t i1 = globalVertexOffset + geom.source.indices[i + 1];
            const std::size_t i2 = globalVertexOffset + geom.source.indices[i + 2];
            out << "f " << i0 << " " << i1 << " " << i2 << "\n";
        }

        globalVertexOffset += geom.source.vertices.size();
    }

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
    return EnsureNavQuery(*ctx);
}

GTANAVVIEWER_API int StreamTilesAround(void* navMesh, Vector3 center, float radius, bool allowBuildIfMissing)
{
    if (!navMesh)
        return 0;
    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
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

static int RunPathfindInternal(ExternNavmeshContext& ctx,
                               const glm::vec3& start,
                               const glm::vec3& end,
                               int flags,
                               int maxPoints,
                               float minEdge,
                               float* outPath,
                               bool* outIsOffMeshLinkNode,
                               bool* outIsNextOffMeshLinkNodeHigher,
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
        outPath[i * 3 + 0] = straight[i * 3 + 0];
        outPath[i * 3 + 1] = straight[i * 3 + 1];
        outPath[i * 3 + 2] = straight[i * 3 + 2];

        if (outIsOffMeshLinkNode)
        {
            const unsigned char nodeFlags = straightFlags[static_cast<size_t>(i)];
            outIsOffMeshLinkNode[i] = (nodeFlags & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
        }

        if (outIsNextOffMeshLinkNodeHigher)
        {
            bool isHigher = false;
            if (i > 0)
            {
                const unsigned char nodeFlags = straightFlags[static_cast<size_t>(i)];
                if ((nodeFlags & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0)
                {
                    const float previousY = straight[(i - 1) * 3 + 1];
                    const float currentY = straight[i * 3 + 1];
                    isHigher = currentY > previousY;
                }
            }
            outIsNextOffMeshLinkNodeHigher[i] = isHigher;
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
                               nullptr,
                               options);
}

GTANAVVIEWER_API int FindPathWithNodeMetadata(void* navMesh,
                                              Vector3 start,
                                              Vector3 target,
                                              int flags,
                                              int maxPoints,
                                              float minEdgeDist,
                                              float* outPath,
                                              bool* outIsOffMeshLinkNode,
                                              bool* outIsNextOffMeshLinkNodeHigher,
                                              int options)
{
    if (!navMesh || !outPath || !outIsOffMeshLinkNode || !outIsNextOffMeshLinkNodeHigher || maxPoints <= 0)
        return 0;

    auto* ctx = static_cast<ExternNavmeshContext*>(navMesh);
    return RunPathfindInternal(*ctx,
                               glm::vec3(start.x, start.y, start.z),
                               glm::vec3(target.x, target.y, target.z),
                               flags,
                               maxPoints,
                               minEdgeDist,
                               outPath,
                               outIsOffMeshLinkNode,
                               outIsNextOffMeshLinkNodeHigher,
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
    AutoOffmeshGenerationParams params = ctx->autoOffmeshParams;
    params.agentRadius = ctx->genSettings.agentRadius;
    params.agentHeight = ctx->genSettings.agentHeight;
    params.maxSlopeDegrees = ctx->genSettings.agentMaxSlope;

    std::vector<OffmeshLink> generated;
    if (!ctx->navData.GenerateAutomaticOffmeshLinks(params, generated))
        return false;

    const unsigned int autoMask = 0xffff0000u;
    std::vector<OffmeshLink> merged;
    const auto& existing = ctx->navData.GetOffmeshLinks();
    merged.reserve(existing.size() + generated.size());
    for (const auto& link : existing)
    {
        //if ( (link.userId & autoMask) != (params.userIdBase & autoMask) )
            merged.push_back(link);
    }
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
