#pragma once

#include <cstdint>
#include <unordered_map>

#include <DetourNavMesh.h>

static constexpr uint32_t TILE_DB_MAGIC = 'G' << 24 | 'T' << 16 | 'D' << 8 | 'B';
static constexpr uint32_t TILE_DB_VERSION = 1;

struct TileDbHeader
{
    uint32_t magic = TILE_DB_MAGIC;
    uint32_t version = TILE_DB_VERSION;
    uint32_t tileCount = 0;
    uint32_t indexOffset = 0;
    dtNavMeshParams navParams{};
};

struct TileDbIndexEntry
{
    int tx = 0;
    int ty = 0;
    uint32_t dataSize = 0;
    uint32_t dataOffset = 0;
    uint64_t geomHash = 0;
};

inline uint64_t MakeTileKey(int tx, int ty)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(tx)) << 32) | static_cast<uint32_t>(ty);
}

bool TileDbLoadIndex(const char* dbPath,
                     dtNavMesh* nav,
                     std::unordered_map<uint64_t, TileDbIndexEntry>& outIndex);

bool TileDbReadTile(const char* dbPath,
                    const TileDbIndexEntry& entry,
                    unsigned char*& outData,
                    int& outSize);

bool TileDbWriteOrUpdateTiles(const char* dbPath,
                              dtNavMesh* nav,
                              const std::unordered_map<uint64_t, uint64_t>& tileHashes);

bool LoadTileFromDb(const char* dbPath,
                    dtNavMesh* nav,
                    int tx,
                    int ty,
                    bool& outLoaded,
                    const std::unordered_map<uint64_t, TileDbIndexEntry>* indexOverride = nullptr);

bool LoadTilesInBoundsFromDb(const char* dbPath,
                             dtNavMesh* nav,
                             const float* bmin,
                             const float* bmax,
                             int& outLoadedCount);