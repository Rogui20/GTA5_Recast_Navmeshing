#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

class MemoryHandler
{
public:
    MemoryHandler();
    ~MemoryHandler();

    void Tick();
    bool SetMonitoringEnabled(bool enabled);
    bool IsMonitoringRequested() const { return monitoringRequested; }
    bool IsMonitoringActive() const { return monitoringActive; }
    std::string GetStatus() const { return statusMessage; }
    std::filesystem::path GetLayoutFilePath() const { return layoutFilePath; }
    std::filesystem::path GetPropHashFile() const { return propHashFile; }
    std::filesystem::path GetObjDirectory() const { return objDirectory; }
    bool SetPropHashFile(const std::filesystem::path& path);
    bool SetObjDirectory(const std::filesystem::path& path);
    bool LoadPropHashMapping();
    bool HasPropHashMapping() const { return !propHashToName.empty(); }
    bool TryResolvePropName(const std::string& hashKey, std::string& outName) const;
    void LoadSavedConfig();

    static constexpr int kGeometrySlotCount = 400;
    static constexpr int kRouteRequestCount = 80;
    static constexpr int kRouteResultPoints = 255;
    static constexpr int kOffmeshLinkCount = 255;
    static constexpr int kBoundingBoxSlotCount = 1;
    static constexpr size_t kModelHashStringSize = 64;

    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct GeometrySlot
    {
        uint64_t modelHash = 0;
        Vector3 position{};
        Vector3 rotation{};
        uint32_t parentId = 0;
        uint8_t update = 0;
        uint8_t remove = 0;
        uint8_t padding[2]{};
    };

    struct RouteRequestSlot
    {
        Vector3 start{};
        Vector3 target{};
        int32_t navmeshSlot = 0;
        uint32_t flags = 0;
        float minEdgeDistance = 0.0f;
        int32_t state = 0; // 0 ready, 1 busy, 2 finished
        uint8_t request = 0;
        uint8_t padding[3]{};
        int32_t pointsCount = 0;
    };

    struct RouteResultPoint
    {
        Vector3 point{};
    };

    struct OffmeshLinkSlot
    {
        Vector3 start{};
        Vector3 end{};
        uint8_t biDir = 0;
        uint8_t enabled = 0;
        uint8_t update = 0;
        uint8_t padding = 0;
    };

    struct BoundingBoxSlot
    {
        Vector3 bmin{};
        Vector3 bmax{};
        uint8_t remove = 0;
        uint8_t update = 0;
        uint8_t padding[2]{};
    };

    bool FetchGeometrySlots(std::vector<GeometrySlot>& outSlots) const;
    bool WriteGeometrySlot(int index, const GeometrySlot& slot) const;
    bool ClearGeometrySlot(int index) const;
    bool HasValidBuffers() const;
    size_t GeometryBufferSize() const;
    bool FetchRouteRequests(std::vector<RouteRequestSlot>& outSlots) const;
    bool WriteRouteRequestSlot(int index, const RouteRequestSlot& slot) const;
    bool WriteRouteResultPoints(int routeIndex, const std::vector<Vector3>& points) const;
    bool HasValidRouteBuffers() const;
    bool FetchOffmeshLinks(std::vector<OffmeshLinkSlot>& outSlots) const;
    bool WriteOffmeshLinkSlot(int index, const OffmeshLinkSlot& slot) const;
    bool FetchBoundingBox(BoundingBoxSlot& outSlot) const;
    bool WriteBoundingBox(const BoundingBoxSlot& slot) const;

private:
    bool EnsureAttached();
    bool FindProcess(uint32_t& pidOut, std::string& nameOut);
    bool AllocateBuffers();
    bool WriteLayoutFile() const;
    bool ZeroRemoteBuffer(uintptr_t address, size_t size) const;
    std::filesystem::path ResolveLayoutFilePath() const;
    std::filesystem::path ResolveConfigFilePath() const;
    std::string AddressToHex(uintptr_t address) const;
    bool IsProcessAlive() const;
    void SaveConfig() const;
    void ClearMappings();
    bool ReadRemote(void* dest, uintptr_t address, size_t size) const;
    bool WriteRemote(uintptr_t address, const void* data, size_t size) const;
    void ReleaseResources(bool resetStatus);

    bool monitoringRequested = false;
    bool monitoringActive = false;
    uint32_t processId = 0;
    std::string processName;
    void* processHandle = nullptr;
    uintptr_t geometryBufferAddress = 0;
    uintptr_t routeRequestBufferAddress = 0;
    uintptr_t routeResultBufferAddress = 0;
    std::filesystem::path layoutFilePath;
    std::filesystem::path configFilePath;
    std::filesystem::path propHashFile;
    std::filesystem::path objDirectory;
    std::string statusMessage;
    std::unordered_map<std::string, std::string> propHashToName;
    uintptr_t offmeshLinkBufferAddress = 0;
    uintptr_t boundingBoxBufferAddress = 0;
};
