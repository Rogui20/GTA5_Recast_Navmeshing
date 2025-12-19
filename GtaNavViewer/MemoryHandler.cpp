#include "MemoryHandler.h"

#include "json.hpp"

#include <SDL.h>

#include <array>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>
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
    layoutFilePath = ResolveLayoutFilePath();
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
        CloseHandle(handle);
    }
#endif

    processHandle = nullptr;
    processId = 0;
    processName.clear();
    geometryBufferAddress = 0;
    routeRequestBufferAddress = 0;
    routeResultBufferAddress = 0;
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

    geometryBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, geometryBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    routeRequestBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, requestBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    routeResultBufferAddress = reinterpret_cast<uintptr_t>(VirtualAllocEx(handle, nullptr, resultBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!geometryBufferAddress || !routeRequestBufferAddress || !routeResultBufferAddress)
    {
        statusMessage = "Falha ao alocar buffers no processo GTA.";
        return false;
    }

    if (!ZeroRemoteBuffer(geometryBufferAddress, geometryBytes) ||
        !ZeroRemoteBuffer(routeRequestBufferAddress, requestBytes) ||
        !ZeroRemoteBuffer(routeResultBufferAddress, resultBytes))
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

    std::ofstream file(layoutFilePath, std::ios::trunc);
    if (!file)
        return false;

    file << layout.dump(4);
    return true;
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
