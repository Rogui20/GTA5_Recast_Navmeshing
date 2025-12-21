#include "MemoryHandler.h"

#include "json.hpp"

#include <SDL.h>

#include <array>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <utility>
#include <vector>

#if defined(_WIN32)
#    include <Windows.h>
#    include <TlHelp32.h>
#endif

namespace
{
    constexpr std::array<const wchar_t*, 2> kProcessNames = {L"GTA5.exe", L"GTA5_Enhanced.exe"};
}

MemoryHandler::MemoryHandler()
{
    static_assert(std::is_standard_layout<GeometrySlot>::value, "GeometrySlot precisa ser standard layout");
    static_assert(std::is_standard_layout<RouteRequestSlot>::value, "RouteRequestSlot precisa ser standard layout");
    static_assert(std::is_standard_layout<RouteResultPoint>::value, "RouteResultPoint precisa ser standard layout");
    static_assert(std::is_standard_layout<OffmeshLinkSlot>::value, "OffmeshLinkSlot precisa ser standard layout");
    static_assert(std::is_standard_layout<OffmeshControlSlot>::value, "OffmeshControlSlot precisa ser standard layout");
    static_assert(std::is_standard_layout<BoundingBoxSlot>::value, "BoundingBoxSlot precisa ser standard layout");
    layoutFilePath = ResolveLayoutFilePath();
    configFilePath = ResolveConfigFilePath();
    LoadSavedConfig();
    statusMessage = "Monitoramento inativo.";
}

MemoryHandler::~MemoryHandler()
{
    ReleaseResources(true);
}

bool MemoryHandler::SetMonitoringEnabled(bool enabled)
{
    monitoringRequested = enabled;
    if (!monitoringRequested)
    {
        ReleaseResources(false);
        statusMessage = "Monitoramento desativado.";
        return true;
    }

    return EnsureAttached();
}

void MemoryHandler::Tick()
{
#if defined(_WIN32)
    if (!monitoringRequested)
        return;

    if (monitoringActive)
    {
        if (!IsProcessAlive())
        {
            ReleaseResources(false);
            statusMessage = "Processo GTA finalizado. Aguardando nova instância...";
        }
        return;
    }

    EnsureAttached();
#else
    if (monitoringRequested)
    {
        statusMessage = "Monitoramento de GTA disponível apenas no Windows.";
    }
#endif
}

bool MemoryHandler::EnsureAttached()
{
#if defined(_WIN32)
    uint32_t pid = 0;
    std::string name;
    if (!FindProcess(pid, name))
    {
        statusMessage = "Nenhum processo GTA5.exe ou GTA5_Enhanced.exe encontrado.";
        return false;
    }

    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
    if (!handle)
    {
        statusMessage = "Falha ao abrir o processo GTA para leitura/escrita.";
        return false;
    }

    processHandle = handle;
    processId = pid;
    processName = name;

    if (!AllocateBuffers())
    {
        ReleaseResources(false);
        return false;
    }

    monitoringActive = true;
    std::ostringstream oss;
    oss << "Monitorando " << processName << " (PID " << processId << ").";
    if (!layoutFilePath.empty())
    {
        oss << " Layout: " << layoutFilePath.string();
    }
    statusMessage = oss.str();
    return true;
#else
    statusMessage = "Monitoramento de GTA disponível apenas no Windows.";
    return false;
#endif
}

void MemoryHandler::ReleaseResources(bool resetStatus)
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (handle)
    {
        if (geometryBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(geometryBufferAddress), 0, MEM_RELEASE);
        }
        if (routeRequestBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(routeRequestBufferAddress), 0, MEM_RELEASE);
        }
        if (routeResultBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(routeResultBufferAddress), 0, MEM_RELEASE);
        }
        if (offmeshLinkBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(offmeshLinkBufferAddress), 0, MEM_RELEASE);
        }
        if (offmeshControlBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(offmeshControlBufferAddress), 0, MEM_RELEASE);
        }
        if (boundingBoxBufferAddress)
        {
            VirtualFreeEx(handle, reinterpret_cast<void*>(boundingBoxBufferAddress), 0, MEM_RELEASE);
        }
        CloseHandle(handle);
    }
#endif

    processHandle = nullptr;
    processId = 0;
    processName.clear();
    geometryBufferAddress = 0;
    routeRequestBufferAddress = 0;
    routeResultBufferAddress = 0;
    offmeshLinkBufferAddress = 0;
    offmeshControlBufferAddress = 0;
    boundingBoxBufferAddress = 0;
    monitoringActive = false;
    if (resetStatus)
    {
        statusMessage = "Monitoramento inativo.";
    }
}

bool MemoryHandler::FindProcess(uint32_t& pidOut, std::string& nameOut)
{
#if defined(_WIN32)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        statusMessage = "Falha ao enumerar processos.";
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(snapshot, &entry))
    {
        CloseHandle(snapshot);
        statusMessage = "Nenhum processo encontrado.";
        return false;
    }

    auto toLower = [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); };

    do
    {
        std::wstring exeName = entry.szExeFile;
        for (auto& candidate : kProcessNames)
        {
            if (exeName.size() != wcslen(candidate))
                continue;

            bool match = true;
            for (size_t i = 0; i < exeName.size(); ++i)
            {
                if (toLower(exeName[i]) != toLower(candidate[i]))
                {
                    match = false;
                    break;
                }
            }

            if (match)
            {
                pidOut = entry.th32ProcessID;
                std::wstring wideName(entry.szExeFile);
                int required = WideCharToMultiByte(CP_UTF8, 0, wideName.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string utf8Name;
                utf8Name.resize(static_cast<size_t>(required > 0 ? required - 1 : 0));
                if (required > 1)
                {
                    WideCharToMultiByte(CP_UTF8, 0, wideName.c_str(), -1, utf8Name.data(), required, nullptr, nullptr);
                }
                nameOut = utf8Name;
                CloseHandle(snapshot);
                return true;
            }
        }
    }
    while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return false;
#else
    (void)pidOut;
    (void)nameOut;
    statusMessage = "Monitoramento de GTA disponível apenas no Windows.";
    return false;
#endif
}

bool MemoryHandler::AllocateBuffers()
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (!handle)
    {
        statusMessage = "Processo GTA não está aberto.";
        return false;
    }

    const size_t geometryBytes = sizeof(GeometrySlot) * static_cast<size_t>(kGeometrySlotCount);
    const size_t requestBytes = sizeof(RouteRequestSlot) * static_cast<size_t>(kRouteRequestCount);
    const size_t resultBytes = sizeof(RouteResultPoint) * static_cast<size_t>(kRouteRequestCount) * static_cast<size_t>(kRouteResultPoints);
    const size_t offmeshBytes = sizeof(OffmeshLinkSlot) * static_cast<size_t>(kOffmeshLinkCount);
    const size_t offmeshControlBytes = sizeof(OffmeshControlSlot) * static_cast<size_t>(kOffmeshControlCount);
    const size_t bboxBytes = sizeof(BoundingBoxSlot) * static_cast<size_t>(kBoundingBoxSlotCount);

    geometryBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, geometryBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    routeRequestBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, requestBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    routeResultBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, resultBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    offmeshLinkBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, offmeshBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    offmeshControlBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, offmeshControlBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    boundingBoxBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, bboxBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!geometryBufferAddress || !routeRequestBufferAddress || !routeResultBufferAddress || !offmeshLinkBufferAddress || !offmeshControlBufferAddress || !boundingBoxBufferAddress)
    {
        statusMessage = "Falha ao alocar buffers no processo GTA.";
        return false;
    }

    if (!ZeroRemoteBuffer(geometryBufferAddress, geometryBytes) ||
        !ZeroRemoteBuffer(routeRequestBufferAddress, requestBytes) ||
        !ZeroRemoteBuffer(routeResultBufferAddress, resultBytes) ||
        !ZeroRemoteBuffer(offmeshLinkBufferAddress, offmeshBytes) ||
        !ZeroRemoteBuffer(offmeshControlBufferAddress, offmeshControlBytes) ||
        !ZeroRemoteBuffer(boundingBoxBufferAddress, bboxBytes))
    {
        statusMessage = "Falha ao inicializar buffers remotos.";
        return false;
    }

    if (!WriteLayoutFile())
    {
        statusMessage = "Buffers alocados, mas falha ao salvar layout JSON.";
    }

    return true;
#else
    statusMessage = "Monitoramento de GTA disponível apenas no Windows.";
    return false;
#endif
}

bool MemoryHandler::WriteLayoutFile() const
{
    if (layoutFilePath.empty())
        return false;

    nlohmann::json layout;
    layout["processName"] = processName;
    layout["pid"] = processId;

    layout["geometry"] = {
        {"address", AddressToHex(geometryBufferAddress)},
        {"count", kGeometrySlotCount},
        {"stride", sizeof(GeometrySlot)},
        {"offsets", {
            {"modelHash", offsetof(GeometrySlot, modelHash)},
            {"position", offsetof(GeometrySlot, position)},
            {"rotation", offsetof(GeometrySlot, rotation)},
            {"parentId", offsetof(GeometrySlot, parentId)},
            {"update", offsetof(GeometrySlot, update)},
            {"delete", offsetof(GeometrySlot, remove)}
        }}
    };

    layout["routeRequests"] = {
        {"address", AddressToHex(routeRequestBufferAddress)},
        {"count", kRouteRequestCount},
        {"stride", sizeof(RouteRequestSlot)},
        {"offsets", {
            {"start", offsetof(RouteRequestSlot, start)},
            {"target", offsetof(RouteRequestSlot, target)},
            {"navmeshSlot", offsetof(RouteRequestSlot, navmeshSlot)},
            {"flags", offsetof(RouteRequestSlot, flags)},
            {"minEdgeDistance", offsetof(RouteRequestSlot, minEdgeDistance)},
            {"state", offsetof(RouteRequestSlot, state)},
            {"request", offsetof(RouteRequestSlot, request)}
        }}
    };

    layout["routeResults"] = {
        {"address", AddressToHex(routeResultBufferAddress)},
        {"routeCount", kRouteRequestCount},
        {"pointsPerRoute", kRouteResultPoints},
        {"stride", sizeof(RouteResultPoint)},
        {"vectorStride", sizeof(Vector3)}
    };

    layout["offmeshLinks"] = {
        {"address", AddressToHex(offmeshLinkBufferAddress)},
        {"count", kOffmeshLinkCount},
        {"stride", sizeof(OffmeshLinkSlot)},
        {"offsets", {
            {"start", offsetof(OffmeshLinkSlot, start)},
            {"end", offsetof(OffmeshLinkSlot, end)},
            {"biDir", offsetof(OffmeshLinkSlot, biDir)}
        }}
    };

    layout["offmeshControl"] = {
        {"address", AddressToHex(offmeshControlBufferAddress)},
        {"count", kOffmeshControlCount},
        {"stride", sizeof(OffmeshControlSlot)},
        {"offsets", {
            {"addAll", offsetof(OffmeshControlSlot, addAll)},
            {"removeAll", offsetof(OffmeshControlSlot, removeAll)}
        }}
    };

    layout["boundingBox"] = {
        {"address", AddressToHex(boundingBoxBufferAddress)},
        {"count", kBoundingBoxSlotCount},
        {"stride", sizeof(BoundingBoxSlot)},
        {"offsets", {
            {"bmin", offsetof(BoundingBoxSlot, bmin)},
            {"bmax", offsetof(BoundingBoxSlot, bmax)},
            {"remove", offsetof(BoundingBoxSlot, remove)},
            {"update", offsetof(BoundingBoxSlot, update)}
        }}
    };

    std::ofstream file(layoutFilePath, std::ios::trunc);
    if (!file)
        return false;

    file << layout.dump(4);
    return true;
}

bool MemoryHandler::SetPropHashFile(const std::filesystem::path& path)
{
    if (path.empty() || !std::filesystem::exists(path))
        return false;

    propHashFile = path;
    SaveConfig();
    return LoadPropHashMapping();
}

bool MemoryHandler::SetObjDirectory(const std::filesystem::path& path)
{
    if (path.empty() || !std::filesystem::exists(path) || !std::filesystem::is_directory(path))
        return false;

    objDirectory = path;
    SaveConfig();
    return true;
}

bool MemoryHandler::LoadPropHashMapping()
{
    ClearMappings();
    if (propHashFile.empty())
        return false;

    std::ifstream file(propHashFile);
    if (!file)
        return false;

    nlohmann::json data;
    try
    {
        file >> data;
    }
    catch (...)
    {
        return false;
    }

    if (!data.is_object())
        return false;

    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it.value().is_string())
        {
            propHashToName[it.key()] = it.value().get<std::string>();
        }
    }

    return !propHashToName.empty();
}

bool MemoryHandler::TryResolvePropName(const std::string& hashKey, std::string& outName) const
{
    auto it = propHashToName.find(hashKey);
    if (it == propHashToName.end())
        return false;

    outName = it->second;
    return true;
}

void MemoryHandler::LoadSavedConfig()
{
    if (configFilePath.empty())
        return;

    std::ifstream file(configFilePath);
    if (!file)
        return;

    std::string propPath;
    std::string objPath;
    std::getline(file, propPath);
    std::getline(file, objPath);

    if (!propPath.empty())
    {
        std::filesystem::path candidate = propPath;
        if (std::filesystem::exists(candidate))
        {
            propHashFile = candidate;
            LoadPropHashMapping();
        }
    }

    if (!objPath.empty())
    {
        std::filesystem::path candidate = objPath;
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
        {
            objDirectory = candidate;
        }
    }
}

bool MemoryHandler::FetchGeometrySlots(std::vector<GeometrySlot>& outSlots) const
{
#if defined(_WIN32)
    if (!HasValidBuffers())
        return false;

    outSlots.resize(static_cast<size_t>(kGeometrySlotCount));
    return ReadRemote(outSlots.data(), geometryBufferAddress, GeometryBufferSize());
#else
    (void)outSlots;
    return false;
#endif
}

bool MemoryHandler::WriteGeometrySlot(int index, const GeometrySlot& slot) const
{
#if defined(_WIN32)
    if (!HasValidBuffers() || index < 0 || index >= kGeometrySlotCount)
        return false;

    const uintptr_t address = geometryBufferAddress + static_cast<uintptr_t>(index) * sizeof(GeometrySlot);
    return WriteRemote(address, &slot, sizeof(GeometrySlot));
#else
    (void)index;
    (void)slot;
    return false;
#endif
}

bool MemoryHandler::ClearGeometrySlot(int index) const
{
#if defined(_WIN32)
    if (!HasValidBuffers() || index < 0 || index >= kGeometrySlotCount)
        return false;

    GeometrySlot zeroSlot{};
    return WriteGeometrySlot(index, zeroSlot);
#else
    (void)index;
    return false;
#endif
}

bool MemoryHandler::HasValidBuffers() const
{
    return monitoringActive && geometryBufferAddress != 0;
}

size_t MemoryHandler::GeometryBufferSize() const
{
    return sizeof(GeometrySlot) * static_cast<size_t>(kGeometrySlotCount);
}

bool MemoryHandler::FetchRouteRequests(std::vector<RouteRequestSlot>& outSlots) const
{
#if defined(_WIN32)
    if (!HasValidRouteBuffers())
        return false;

    outSlots.resize(static_cast<size_t>(kRouteRequestCount));
    const size_t sizeBytes = sizeof(RouteRequestSlot) * static_cast<size_t>(kRouteRequestCount);
    return ReadRemote(outSlots.data(), routeRequestBufferAddress, sizeBytes);
#else
    (void)outSlots;
    return false;
#endif
}

bool MemoryHandler::WriteRouteRequestSlot(int index, const RouteRequestSlot& slot) const
{
#if defined(_WIN32)
    if (!HasValidRouteBuffers() || index < 0 || index >= kRouteRequestCount)
        return false;

    const uintptr_t address = routeRequestBufferAddress + static_cast<uintptr_t>(index) * sizeof(RouteRequestSlot);
    return WriteRemote(address, &slot, sizeof(RouteRequestSlot));
#else
    (void)index;
    (void)slot;
    return false;
#endif
}

bool MemoryHandler::WriteRouteResultPoints(int routeIndex, const std::vector<Vector3>& points) const
{
#if defined(_WIN32)
    if (!HasValidRouteBuffers() || routeIndex < 0 || routeIndex >= kRouteRequestCount)
        return false;

    const size_t maxPoints = static_cast<size_t>(kRouteResultPoints);
    std::vector<RouteResultPoint> buffer(maxPoints);
    const size_t copyCount = (std::min)(points.size(), maxPoints);
    for (size_t i = 0; i < copyCount; ++i)
    {
        buffer[i].point = points[i];
    }

    const uintptr_t address = routeResultBufferAddress +
        static_cast<uintptr_t>(routeIndex) * static_cast<uintptr_t>(kRouteResultPoints) * sizeof(RouteResultPoint);
    const size_t bytes = buffer.size() * sizeof(RouteResultPoint);
    return WriteRemote(address, buffer.data(), bytes);
#else
    (void)routeIndex;
    (void)points;
    return false;
#endif
}

bool MemoryHandler::HasValidRouteBuffers() const
{
    return monitoringActive && routeRequestBufferAddress != 0 && routeResultBufferAddress != 0;
}

bool MemoryHandler::FetchOffmeshLinks(std::vector<OffmeshLinkSlot>& outSlots) const
{
#if defined(_WIN32)
    if (!monitoringActive || offmeshLinkBufferAddress == 0)
        return false;

    outSlots.resize(static_cast<size_t>(kOffmeshLinkCount));
    const size_t bytes = sizeof(OffmeshLinkSlot) * static_cast<size_t>(kOffmeshLinkCount);
    return ReadRemote(outSlots.data(), offmeshLinkBufferAddress, bytes);
#else
    (void)outSlots;
    return false;
#endif
}

bool MemoryHandler::WriteOffmeshLinkSlot(int index, const OffmeshLinkSlot& slot) const
{
#if defined(_WIN32)
    if (!monitoringActive || offmeshLinkBufferAddress == 0 || index < 0 || index >= kOffmeshLinkCount)
        return false;

    const uintptr_t address = offmeshLinkBufferAddress + static_cast<uintptr_t>(index) * sizeof(OffmeshLinkSlot);
    return WriteRemote(address, &slot, sizeof(OffmeshLinkSlot));
#else
    (void)index;
    (void)slot;
    return false;
#endif
}

bool MemoryHandler::FetchOffmeshControl(OffmeshControlSlot& outSlot) const
{
#if defined(_WIN32)
    if (!monitoringActive || offmeshControlBufferAddress == 0)
        return false;

    return ReadRemote(&outSlot, offmeshControlBufferAddress, sizeof(OffmeshControlSlot));
#else
    (void)outSlot;
    return false;
#endif
}

bool MemoryHandler::WriteOffmeshControl(const OffmeshControlSlot& slot) const
{
#if defined(_WIN32)
    if (!monitoringActive || offmeshControlBufferAddress == 0)
        return false;

    return WriteRemote(offmeshControlBufferAddress, &slot, sizeof(OffmeshControlSlot));
#else
    (void)slot;
    return false;
#endif
}

bool MemoryHandler::FetchBoundingBox(BoundingBoxSlot& outSlot) const
{
#if defined(_WIN32)
    if (!monitoringActive || boundingBoxBufferAddress == 0)
        return false;

    return ReadRemote(&outSlot, boundingBoxBufferAddress, sizeof(BoundingBoxSlot));
#else
    (void)outSlot;
    return false;
#endif
}

bool MemoryHandler::WriteBoundingBox(const BoundingBoxSlot& slot) const
{
#if defined(_WIN32)
    if (!monitoringActive || boundingBoxBufferAddress == 0)
        return false;

    return WriteRemote(boundingBoxBufferAddress, &slot, sizeof(BoundingBoxSlot));
#else
    (void)slot;
    return false;
#endif
}

bool MemoryHandler::ZeroRemoteBuffer(uintptr_t address, size_t size) const
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (!handle || !address || size == 0)
        return false;

    std::vector<uint8_t> zeros(size, 0);
    SIZE_T written = 0;
    if (!WriteProcessMemory(handle, reinterpret_cast<void*>(address), zeros.data(), size, &written))
        return false;

    return written == size;
#else
    (void)address;
    (void)size;
    return false;
#endif
}

std::filesystem::path MemoryHandler::ResolveLayoutFilePath() const
{
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::filesystem::path result = std::filesystem::path(basePath) / "gta_tracker_layout.json";
        SDL_free(basePath);
        return result;
    }

    return std::filesystem::current_path() / "gta_tracker_layout.json";
}

std::filesystem::path MemoryHandler::ResolveConfigFilePath() const
{
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::filesystem::path result = std::filesystem::path(basePath) / "gta_tracker_config.txt";
        SDL_free(basePath);
        return result;
    }

    return std::filesystem::current_path() / "gta_tracker_config.txt";
}

std::string MemoryHandler::AddressToHex(uintptr_t address) const
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << address;
    return oss.str();
}

bool MemoryHandler::IsProcessAlive() const
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (!handle)
        return false;

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(handle, &exitCode))
        return false;

    return exitCode == STILL_ACTIVE;
#else
    return false;
#endif
}

void MemoryHandler::SaveConfig() const
{
    if (configFilePath.empty())
        return;

    std::ofstream file(configFilePath, std::ios::trunc);
    if (!file)
        return;

    file << propHashFile.string() << "\n";
    file << objDirectory.string() << "\n";
}

void MemoryHandler::ClearMappings()
{
    propHashToName.clear();
}

bool MemoryHandler::ReadRemote(void* dest, uintptr_t address, size_t size) const
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (!handle || !address || size == 0)
        return false;

    SIZE_T read = 0;
    if (!ReadProcessMemory(handle, reinterpret_cast<void*>(address), dest, size, &read))
        return false;

    return read == size;
#else
    (void)dest;
    (void)address;
    (void)size;
    return false;
#endif
}

bool MemoryHandler::WriteRemote(uintptr_t address, const void* data, size_t size) const
{
#if defined(_WIN32)
    HANDLE handle = static_cast<HANDLE>(processHandle);
    if (!handle || !address || size == 0)
        return false;

    SIZE_T written = 0;
    if (!WriteProcessMemory(handle, reinterpret_cast<void*>(address), data, size, &written))
        return false;

    return written == size;
#else
    (void)address;
    (void)data;
    (void)size;
    return false;
#endif
}
