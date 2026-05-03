#include "NavMesh_TileCacheGridDB.h"

#include "json.hpp"

#include <DetourAlloc.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

namespace
{
    bool AreNavParamsCompatibleGrid(const dtNavMeshParams& expected, const dtNavMeshParams& current)
    {
        constexpr float eps = 1e-3f;
        for (int i = 0; i < 3; ++i)
            if (std::fabs(expected.orig[i] - current.orig[i]) > eps) return false;
        if (std::fabs(expected.tileWidth - current.tileWidth) > eps) return false;
        if (std::fabs(expected.tileHeight - current.tileHeight) > eps) return false;
        return current.maxTiles >= expected.maxTiles && current.maxPolys >= expected.maxPolys;
    }

    std::filesystem::path TilePath(const std::filesystem::path& root, int tx, int ty)
    {
        return root / "tiles" / std::to_string(tx) / (std::to_string(ty) + ".tile");
    }
}

bool TileGridDbDeleteTile(const char* rootPath, int tx, int ty)
{
    if (!rootPath) return false;
    std::error_code ec;
    std::filesystem::remove(TilePath(rootPath, tx, ty), ec);
    return true;
}

bool TileGridDbWriteOrUpdateTiles(const char* rootPath,
                                  dtNavMesh* nav,
                                  const std::unordered_map<uint64_t, uint64_t>& tileHashes,
                                  const std::unordered_set<uint64_t>* onlyTileKeysToUpdate)
{
    if (!rootPath || !nav) return false;
    const std::filesystem::path root(rootPath);
    std::filesystem::create_directories(root / "tiles");

    const dtNavMeshParams* params = nav->getParams();
    int saved = 0, deleted = 0;

    auto processKey = [&](uint64_t key)
    {
        const int tx = static_cast<int>(key >> 32);
        const int ty = static_cast<int>(key & 0xffffffffu);
        const dtTileRef ref = nav->getTileRefAt(tx, ty, 0);
        if (!ref)
        {
            TileGridDbDeleteTile(rootPath, tx, ty);
            ++deleted;
            return;
        }

        const dtMeshTile* tile = nav->getTileByRef(ref);
        if (!tile || !tile->data || tile->dataSize <= 0)
        {
            TileGridDbDeleteTile(rootPath, tx, ty);
            ++deleted;
            return;
        }

        const auto outPath = TilePath(root, tx, ty);
        std::filesystem::create_directories(outPath.parent_path());
        const auto tmpPath = outPath.string() + ".tmp";
        FILE* fp = fopen(tmpPath.c_str(), "wb");
        if (!fp)
        {
            printf("[WorldTile][GridDB][erro] open tmp failed tx=%d ty=%d\n", tx, ty);
            return;
        }

        TileGridDbFileHeader h{};
        h.tx = tx; h.ty = ty; h.dataSize = static_cast<uint32_t>(tile->dataSize);
        auto it = tileHashes.find(key);
        h.geomHash = it != tileHashes.end() ? it->second : 0;

        bool ok = fwrite(&h, sizeof(h), 1, fp) == 1 && fwrite(tile->data, tile->dataSize, 1, fp) == 1;
        fclose(fp);
        if (!ok)
        {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            printf("[WorldTile][GridDB][erro] write failed tx=%d ty=%d\n", tx, ty);
            return;
        }
        std::error_code ec;
        std::filesystem::rename(tmpPath, outPath, ec);
        if (ec)
        {
            std::filesystem::remove(outPath, ec);
            ec.clear();
            std::filesystem::rename(tmpPath, outPath, ec);
        }
        if (ec)
        {
            printf("[WorldTile][GridDB][erro] rename failed tx=%d ty=%d\n", tx, ty);
            return;
        }
        ++saved;
    };

    if (onlyTileKeysToUpdate)
    {
        for (uint64_t key : *onlyTileKeysToUpdate) processKey(key);
    }
    else
    {
        for (const auto& kv : tileHashes) processKey(kv.first);
    }

    nlohmann::json j;
    j["version"] = TILE_GRID_DB_VERSION;
    j["tileCount"] = saved;
    if (params)
    {
        j["navParams"] = {
            {"orig", {params->orig[0], params->orig[1], params->orig[2]}},
            {"tileWidth", params->tileWidth}, {"tileHeight", params->tileHeight},
            {"maxTiles", params->maxTiles}, {"maxPolys", params->maxPolys}
        };
    }
    std::ofstream out(root / "manifest.json");
    out << j.dump(2);

    printf("[WorldTile][GridDB] saved=%d deleted=%d root=%s\n", saved, deleted, root.string().c_str());
    return true;
}

bool TileGridDbLoadIndex(const char* rootPath,
                         dtNavMesh* nav,
                         std::unordered_map<uint64_t, TileDbIndexEntry>& outIndex)
{
    outIndex.clear();
    if (!rootPath || !nav) return false;
    const std::filesystem::path root(rootPath);

    const auto manifest = root / "manifest.json";
    if (std::filesystem::exists(manifest))
    {
        try
        {
            nlohmann::json j; std::ifstream in(manifest); in >> j;
            if (j.contains("navParams"))
            {
                dtNavMeshParams expected{};
                auto np = j["navParams"];
                if (np.contains("orig") && np["orig"].is_array() && np["orig"].size() == 3)
                {
                    expected.orig[0] = np["orig"][0].get<float>();
                    expected.orig[1] = np["orig"][1].get<float>();
                    expected.orig[2] = np["orig"][2].get<float>();
                }
                expected.tileWidth = np.value("tileWidth", 0.0f);
                expected.tileHeight = np.value("tileHeight", 0.0f);
                expected.maxTiles = np.value("maxTiles", 0);
                expected.maxPolys = np.value("maxPolys", 0);
                const dtNavMeshParams* cur = nav->getParams();
                if (!cur || !AreNavParamsCompatibleGrid(expected, *cur))
                    return false;
            }
        }
        catch (...) { return false; }
    }

    const auto tilesRoot = root / "tiles";
    if (!std::filesystem::exists(tilesRoot)) return false;
    for (const auto& txDir : std::filesystem::directory_iterator(tilesRoot))
    {
        if (!txDir.is_directory()) continue;
        for (const auto& file : std::filesystem::directory_iterator(txDir.path()))
        {
            if (!file.is_regular_file() || file.path().extension() != ".tile") continue;
            FILE* fp = fopen(file.path().string().c_str(), "rb");
            if (!fp) continue;
            TileGridDbFileHeader h{};
            bool ok = fread(&h, sizeof(h), 1, fp) == 1;
            fclose(fp);
            if (!ok || h.magic != TILE_GRID_DB_MAGIC || h.version != TILE_GRID_DB_VERSION) continue;
            TileDbIndexEntry e{};
            e.tx = h.tx; e.ty = h.ty; e.geomHash = h.geomHash; e.dataSize = h.dataSize; e.dataOffset = sizeof(TileGridDbFileHeader);
            outIndex[MakeTileKey(e.tx, e.ty)] = e;
        }
    }
    printf("[WorldTile][GridDB] index tiles=%zu root=%s\n", outIndex.size(), root.string().c_str());
    return true;
}

bool TileGridDbReadTile(const char* rootPath, int tx, int ty, uint64_t* outGeomHash, unsigned char*& outData, int& outSize)
{
    outData = nullptr; outSize = 0; if (outGeomHash) *outGeomHash = 0;
    if (!rootPath) return false;
    auto path = TilePath(rootPath, tx, ty);
    FILE* fp = fopen(path.string().c_str(), "rb");
    if (!fp) return false;
    TileGridDbFileHeader h{};
    if (fread(&h, sizeof(h), 1, fp) != 1 || h.magic != TILE_GRID_DB_MAGIC || h.version != TILE_GRID_DB_VERSION || h.tx != tx || h.ty != ty)
    { fclose(fp); return false; }
    if (h.dataSize == 0 || h.dataSize > static_cast<uint32_t>(std::numeric_limits<int>::max())) { fclose(fp); return false; }
    unsigned char* data = static_cast<unsigned char*>(dtAlloc(h.dataSize, DT_ALLOC_PERM));
    if (!data) { fclose(fp); return false; }
    if (fread(data, h.dataSize, 1, fp) != 1) { dtFree(data); fclose(fp); return false; }
    fclose(fp);
    if (outGeomHash) *outGeomHash = h.geomHash;
    outData = data; outSize = static_cast<int>(h.dataSize);
    return true;
}

bool TileGridDbLoadTile(const char* rootPath, dtNavMesh* nav, int tx, int ty, bool& outLoaded)
{
    outLoaded = false;
    if (!rootPath || !nav) return false;
    unsigned char* data = nullptr; int size = 0; uint64_t hash = 0;
    if (!TileGridDbReadTile(rootPath, tx, ty, &hash, data, size)) return false;
    const dtStatus st = nav->addTile(data, size, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(st)) { dtFree(data); return false; }
    outLoaded = true;
    printf("[WorldTile][GridDB] loaded tile %d,%d size=%d hash=%llu\n", tx, ty, size, static_cast<unsigned long long>(hash));
    return true;
}
