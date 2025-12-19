#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>

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

    static constexpr int kGeometrySlotCount = 400;
    static constexpr int kRouteRequestCount = 80;
    static constexpr int kRouteResultPoints = 255;
    static constexpr size_t kModelHashStringSize = 64;

private:
    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct GeometrySlot
    {
        char modelHash[kModelHashStringSize]{};
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
        uint32_t flags = 0;
        float minEdgeDistance = 0.0f;
        int32_t state = 0; // 0 ready, 1 busy, 2 finished
        uint8_t request = 0;
        uint8_t padding[3]{};
    };

    struct RouteResultPoint
    {
        Vector3 point{};
    };

    bool EnsureAttached();
    void ReleaseResources(bool resetStatus);
    bool FindProcess(uint32_t& pidOut, std::string& nameOut);
    bool AllocateBuffers();
    bool WriteLayoutFile() const;
    bool ZeroRemoteBuffer(uintptr_t address, size_t size) const;
    std::filesystem::path ResolveLayoutFilePath() const;
    std::string AddressToHex(uintptr_t address) const;
    bool IsProcessAlive() const;

    bool monitoringRequested = false;
    bool monitoringActive = false;
    uint32_t processId = 0;
    std::string processName;
    void* processHandle = nullptr;
    uintptr_t geometryBufferAddress = 0;
    uintptr_t routeRequestBufferAddress = 0;
    uintptr_t routeResultBufferAddress = 0;
    std::filesystem::path layoutFilePath;
    std::string statusMessage;
};
