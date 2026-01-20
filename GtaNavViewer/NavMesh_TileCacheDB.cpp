#include "NavMesh_TileCacheDB.h"

#include <DetourNavMesh.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

namespace
{
    bool ReadHeader(FILE* fp, TileDbHeader& outHeader)
    {
        if (!fp)
            return false;
        if (fread(&outHeader, sizeof(TileDbHeader), 1, fp) != 1)
            return false;
        if (outHeader.magic != TILE_DB_MAGIC || outHeader.version != TILE_DB_VERSION)
        {
            printf("[NavMeshData] Tile DB incompatível (magic/version).\n");
            return false;
        }
        return true;
    }
}

bool TileDbLoadIndex(const char* dbPath,
                     dtNavMesh* nav,
                     std::unordered_map<uint64_t, TileDbIndexEntry>& outIndex)
{
    outIndex.clear();
    if (!dbPath || !nav)
        return false;

    FILE* fp = fopen(dbPath, "rb");
    if (!fp)
        return false;

    TileDbHeader header{};
    if (!ReadHeader(fp, header))
    {
        fclose(fp);
        return false;
    }

    if (memcmp(&header.navParams, nav->getParams(), sizeof(dtNavMeshParams)) != 0)
    {
        fclose(fp);
        printf("[NavMeshData] Tile DB incompatível com parametros atuais.\n");
        return false;
    }

    if (fseek(fp, static_cast<long>(header.indexOffset), SEEK_SET) != 0)
    {
        fclose(fp);
        return false;
    }

    outIndex.reserve(header.tileCount * 2u);
    for (uint32_t i = 0; i < header.tileCount; ++i)
    {
        TileDbIndexEntry entry{};
        if (fread(&entry, sizeof(TileDbIndexEntry), 1, fp) != 1)
        {
            fclose(fp);
            outIndex.clear();
            return false;
        }
        outIndex.emplace(MakeTileKey(entry.tx, entry.ty), entry);
    }

    fclose(fp);
    return true;
}

bool TileDbReadTile(const char* dbPath,
                    const TileDbIndexEntry& entry,
                    unsigned char*& outData,
                    int& outSize)
{
    outData = nullptr;
    outSize = 0;
    if (!dbPath)
        return false;

    FILE* fp = fopen(dbPath, "rb");
    if (!fp)
        return false;

    if (fseek(fp, static_cast<long>(entry.dataOffset), SEEK_SET) != 0)
    {
        fclose(fp);
        return false;
    }

    unsigned char* data = static_cast<unsigned char*>(dtAlloc(entry.dataSize, DT_ALLOC_PERM));
    if (!data)
    {
        fclose(fp);
        return false;
    }

    if (fread(data, entry.dataSize, 1, fp) != 1)
    {
        dtFree(data);
        fclose(fp);
        return false;
    }

    fclose(fp);
    outData = data;
    outSize = static_cast<int>(entry.dataSize);
    return true;
}

bool TileDbWriteOrUpdateTiles(const char* dbPath,
                              dtNavMesh* nav,
                              const std::unordered_map<uint64_t, uint64_t>& tileHashes)
{
    if (!dbPath || !nav)
        return false;

    std::filesystem::path path(dbPath);
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    std::vector<TileDbIndexEntry> indexEntries;
    std::vector<const unsigned char*> tileData;
    indexEntries.reserve(nav->getMaxTiles());
    tileData.reserve(nav->getMaxTiles());

    for (int i = 0; i < nav->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = nav->getTile(i);
        if (!tile || !tile->header || !tile->data || tile->dataSize == 0)
            continue;

        TileDbIndexEntry entry{};
        entry.tx = tile->header->x;
        entry.ty = tile->header->y;
        entry.dataSize = static_cast<uint32_t>(tile->dataSize);
        const uint64_t key = MakeTileKey(entry.tx, entry.ty);
        const auto itHash = tileHashes.find(key);
        entry.geomHash = itHash != tileHashes.end() ? itHash->second : 0;

        indexEntries.push_back(entry);
        tileData.push_back(tile->data);
    }

    TileDbHeader header{};
    header.magic = TILE_DB_MAGIC;
    header.version = TILE_DB_VERSION;
    header.tileCount = static_cast<uint32_t>(indexEntries.size());
    header.indexOffset = 0;
    memcpy(&header.navParams, nav->getParams(), sizeof(dtNavMeshParams));

    FILE* fp = fopen(dbPath, "wb");
    if (!fp)
        return false;

    bool ok = fwrite(&header, sizeof(TileDbHeader), 1, fp) == 1;

    for (size_t i = 0; ok && i < indexEntries.size(); ++i)
    {
        const long offset = ftell(fp);
        if (offset < 0)
        {
            ok = false;
            break;
        }
        indexEntries[i].dataOffset = static_cast<uint32_t>(offset);
        ok = fwrite(tileData[i], indexEntries[i].dataSize, 1, fp) == 1;
    }

    if (ok)
    {
        const long indexOffset = ftell(fp);
        if (indexOffset < 0 || static_cast<uint64_t>(indexOffset) > std::numeric_limits<uint32_t>::max())
        {
            ok = false;
        }
        else
        {
            header.indexOffset = static_cast<uint32_t>(indexOffset);
            for (const auto& entry : indexEntries)
            {
                if (!ok)
                    break;
                ok = fwrite(&entry, sizeof(TileDbIndexEntry), 1, fp) == 1;
            }
        }
    }

    if (ok)
    {
        if (fseek(fp, 0, SEEK_SET) != 0)
        {
            ok = false;
        }
        else
        {
            ok = fwrite(&header, sizeof(TileDbHeader), 1, fp) == 1;
        }
    }

    fclose(fp);

    if (ok)
        printf("[NavMeshData] Tile DB salvo em %s (%zu tiles)\n", dbPath, indexEntries.size());

    return ok;
}

bool LoadTileFromDb(const char* dbPath,
                    dtNavMesh* nav,
                    int tx,
                    int ty,
                    bool& outLoaded)
{
    outLoaded = false;
    if (!dbPath || !nav)
        return false;

    std::unordered_map<uint64_t, TileDbIndexEntry> index;
    if (!TileDbLoadIndex(dbPath, nav, index))
        return false;

    const uint64_t key = MakeTileKey(tx, ty);
    const auto it = index.find(key);
    if (it == index.end())
        return true;

    unsigned char* data = nullptr;
    int dataSize = 0;
    if (!TileDbReadTile(dbPath, it->second, data, dataSize))
        return false;

    dtStatus status = nav->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status))
    {
        dtFree(data);
        printf("[NavMeshData] Falha ao adicionar tile do DB (%d,%d) status=0x%x\n", tx, ty, status);
        return false;
    }

    outLoaded = true;
    return true;
}

bool LoadTilesInBoundsFromDb(const char* dbPath,
                             dtNavMesh* nav,
                             const float* bmin,
                             const float* bmax,
                             int& outLoadedCount)
{
    outLoadedCount = 0;
    if (!dbPath || !nav || !bmin || !bmax)
        return false;

    std::unordered_map<uint64_t, TileDbIndexEntry> index;
    if (!TileDbLoadIndex(dbPath, nav, index))
        return false;

    const dtNavMeshParams* params = nav->getParams();
    const float tileWidth = params->tileWidth;

    const int minTx = static_cast<int>(floorf((bmin[0] - params->orig[0]) / tileWidth));
    const int minTy = static_cast<int>(floorf((bmin[2] - params->orig[2]) / tileWidth));
    const int maxTx = static_cast<int>(floorf((bmax[0] - params->orig[0]) / tileWidth));
    const int maxTy = static_cast<int>(floorf((bmax[2] - params->orig[2]) / tileWidth));

    for (const auto& pair : index)
    {
        const TileDbIndexEntry& entry = pair.second;
        if (entry.tx < minTx || entry.tx > maxTx || entry.ty < minTy || entry.ty > maxTy)
            continue;
        if (nav->getTileRefAt(entry.tx, entry.ty, 0) != 0)
            continue;

        unsigned char* data = nullptr;
        int dataSize = 0;
        if (!TileDbReadTile(dbPath, entry, data, dataSize))
            continue;

        dtStatus status = nav->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, nullptr);
        if (dtStatusFailed(status))
        {
            dtFree(data);
            continue;
        }
        ++outLoadedCount;
    }

    return true;
}
