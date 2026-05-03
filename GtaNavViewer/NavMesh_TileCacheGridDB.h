#pragma once

#include "NavMesh_TileCacheDB.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include <DetourNavMesh.h>

static constexpr uint32_t TILE_GRID_DB_MAGIC = 'G' << 24 | 'T' << 16 | 'G' << 8 | 'D';
static constexpr uint32_t TILE_GRID_DB_VERSION = 1;

struct TileGridDbFileHeader
{
    uint32_t magic = TILE_GRID_DB_MAGIC;
    uint32_t version = TILE_GRID_DB_VERSION;
    int32_t tx = 0;
    int32_t ty = 0;
    uint64_t geomHash = 0;
    uint32_t dataSize = 0;
};

bool TileGridDbWriteOrUpdateTiles(const char* rootPath,
                                  dtNavMesh* nav,
                                  const std::unordered_map<uint64_t, uint64_t>& tileHashes,
                                  const std::unordered_set<uint64_t>* onlyTileKeysToUpdate);

bool TileGridDbLoadIndex(const char* rootPath,
                         dtNavMesh* nav,
                         std::unordered_map<uint64_t, TileDbIndexEntry>& outIndex);

bool TileGridDbReadTile(const char* rootPath,
                        int tx,
                        int ty,
                        uint64_t* outGeomHash,
                        unsigned char*& outData,
                        int& outSize);

bool TileGridDbLoadTile(const char* rootPath,
                        dtNavMesh* nav,
                        int tx,
                        int ty,
                        bool& outLoaded);

bool TileGridDbDeleteTile(const char* rootPath,
                          int tx,
                          int ty);
