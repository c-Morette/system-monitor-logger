#if defined(_WIN32)

// Alvo minimo Windows 7: necessario para PdhAddEnglishCounterW (contadores PDH
// em ingles, independentes do idioma). Definido antes de qualquer header Win32.
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601
#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0601

#include "platform/WindowsMetricsProvider.hpp"
#include "utils/TimeHelper.hpp"

#include <windows.h>
#include <winioctl.h>
#include <pdh.h>

#include <algorithm>

namespace
{
    std::uint64_t ToU64(const FILETIME& ft)
    {
        ULARGE_INTEGER value;
        value.LowPart = ft.dwLowDateTime;
        value.HighPart = ft.dwHighDateTime;
        return value.QuadPart;
    }

    wchar_t SystemDriveLetter()
    {
        wchar_t windowsDir[MAX_PATH];
        if (GetWindowsDirectoryW(windowsDir, MAX_PATH) > 0)
        {
            return windowsDir[0];
        }
        return L'C';
    }
}

WindowsMetricsProvider::~WindowsMetricsProvider()
{
    if (m_pdhQuery != nullptr)
    {
        PdhCloseQuery(static_cast<PDH_HQUERY>(m_pdhQuery));
        m_pdhQuery = nullptr;
    }
}

double WindowsMetricsProvider::GetCpuPercent()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
    {
        return 0.0;
    }

    const CpuTimes current{ToU64(idleTime), ToU64(kernelTime), ToU64(userTime)};
    if (!m_lastCpu)
    {
        m_lastCpu = current;
        return 0.0;
    }

    const CpuTimes previous = *m_lastCpu;
    m_lastCpu = current;

    const std::uint64_t idleDelta = current.idle - previous.idle;
    const std::uint64_t kernelDelta = current.kernel - previous.kernel;
    const std::uint64_t userDelta = current.user - previous.user;
    const std::uint64_t totalDelta = kernelDelta + userDelta; // kernel ja inclui idle

    if (totalDelta == 0)
    {
        return 0.0;
    }

    const std::uint64_t busy = totalDelta - idleDelta;
    return std::clamp(static_cast<double>(busy) * 100.0 / static_cast<double>(totalDelta), 0.0, 100.0);
}

void WindowsMetricsProvider::GetMemory(double& totalMb, double& usedMb, double& usedPercent)
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
    {
        totalMb = usedMb = usedPercent = 0.0;
        return;
    }

    totalMb = static_cast<double>(status.ullTotalPhys) / 1024.0 / 1024.0;
    const double freeMb = static_cast<double>(status.ullAvailPhys) / 1024.0 / 1024.0;
    usedMb = std::max(0.0, totalMb - freeMb);
    usedPercent = totalMb <= 0.0 ? 0.0 : usedMb * 100.0 / totalMb;
}

void WindowsMetricsProvider::GetDisk(double& totalMb, double& freeMb, double& usedPercent)
{
    const wchar_t root[4] = {SystemDriveLetter(), L':', L'\\', L'\0'};

    ULARGE_INTEGER freeAvailable, totalBytes, totalFree;
    if (!GetDiskFreeSpaceExW(root, &freeAvailable, &totalBytes, &totalFree))
    {
        totalMb = freeMb = usedPercent = 0.0;
        return;
    }

    totalMb = static_cast<double>(totalBytes.QuadPart) / 1024.0 / 1024.0;
    freeMb = static_cast<double>(freeAvailable.QuadPart) / 1024.0 / 1024.0;
    usedPercent = totalMb <= 0.0 ? 0.0 : (totalMb - freeMb) * 100.0 / totalMb;
}

// Abre a consulta PDH e registra os contadores (em ingles, independente do
// idioma do Windows). Funciona de Windows Vista/7 ate o 11, sem exigir admin.
bool WindowsMetricsProvider::EnsurePdh()
{
    if (m_pdhTried)
    {
        return m_pdhReady;
    }
    m_pdhTried = true;

    PDH_HQUERY query = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
    {
        return false;
    }

    PDH_HCOUNTER read = nullptr;
    PDH_HCOUNTER write = nullptr;
    PDH_HCOUNTER active = nullptr;

    const bool ok =
        PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &read) == ERROR_SUCCESS &&
        PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &write) == ERROR_SUCCESS &&
        PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &active) == ERROR_SUCCESS;

    if (!ok)
    {
        PdhCloseQuery(query);
        return false;
    }

    m_pdhQuery = query;
    m_pdhRead = read;
    m_pdhWrite = write;
    m_pdhActive = active;
    m_pdhReady = true;
    m_pdhCollections = 0;
    return true;
}

bool WindowsMetricsProvider::GetDiskIoPdh(double& readMbPerSecond, double& writeMbPerSecond, double& activePercent)
{
    if (!EnsurePdh())
    {
        return false;
    }

    if (PdhCollectQueryData(static_cast<PDH_HQUERY>(m_pdhQuery)) != ERROR_SUCCESS)
    {
        return false;
    }

    // Contadores de taxa precisam de pelo menos duas coletas para terem valor.
    if (m_pdhCollections == 0)
    {
        ++m_pdhCollections;
        return true; // disponivel, mas ainda sem taxa -> mantem zeros desta amostra
    }
    ++m_pdhCollections;

    auto formatted = [](void* counter, double& outValue) {
        PDH_FMT_COUNTERVALUE value{};
        const PDH_STATUS status = PdhGetFormattedCounterValue(
            static_cast<PDH_HCOUNTER>(counter), PDH_FMT_DOUBLE, nullptr, &value);
        if (status == ERROR_SUCCESS && value.CStatus == ERROR_SUCCESS)
        {
            outValue = value.doubleValue;
        }
    };

    double readBytes = 0.0, writeBytes = 0.0, active = 0.0;
    formatted(m_pdhRead, readBytes);
    formatted(m_pdhWrite, writeBytes);
    formatted(m_pdhActive, active);

    readMbPerSecond = readBytes / 1024.0 / 1024.0;
    writeMbPerSecond = writeBytes / 1024.0 / 1024.0;
    // % Disk Time no _Total pode passar de 100 (soma de varios discos); limita.
    activePercent = std::clamp(active, 0.0, 100.0);
    return true;
}

// Fallback usado apenas se o PDH nao estiver disponivel: le os contadores do
// volume do sistema via IOCTL_DISK_PERFORMANCE.
void WindowsMetricsProvider::GetDiskIoIoctl(double& readMbPerSecond, double& writeMbPerSecond)
{
    const wchar_t path[7] = {L'\\', L'\\', L'.', L'\\', SystemDriveLetter(), L':', L'\0'};
    HANDLE handle = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DISK_PERFORMANCE performance{};
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_DISK_PERFORMANCE, nullptr, 0,
                                    &performance, sizeof(performance), &bytesReturned, nullptr);
    CloseHandle(handle);
    if (!ok)
    {
        return;
    }

    const DiskIoSnapshot current{
        std::chrono::steady_clock::now(),
        static_cast<std::uint64_t>(performance.BytesRead.QuadPart),
        static_cast<std::uint64_t>(performance.BytesWritten.QuadPart)};

    if (!m_lastDiskIo)
    {
        m_lastDiskIo = current;
        return;
    }

    const DiskIoSnapshot previous = *m_lastDiskIo;
    m_lastDiskIo = current;

    const double elapsedSeconds =
        std::chrono::duration<double>(current.timestamp - previous.timestamp).count();
    if (elapsedSeconds <= 0.0)
    {
        return;
    }

    const std::uint64_t readDelta =
        current.readBytes >= previous.readBytes ? current.readBytes - previous.readBytes : 0;
    const std::uint64_t writeDelta =
        current.writeBytes >= previous.writeBytes ? current.writeBytes - previous.writeBytes : 0;

    readMbPerSecond = static_cast<double>(readDelta) / 1024.0 / 1024.0 / elapsedSeconds;
    writeMbPerSecond = static_cast<double>(writeDelta) / 1024.0 / 1024.0 / elapsedSeconds;
}

void WindowsMetricsProvider::GetDiskIo(double& readMbPerSecond, double& writeMbPerSecond, double& activePercent)
{
    readMbPerSecond = 0.0;
    writeMbPerSecond = 0.0;
    activePercent = 0.0;

    // Caminho principal: PDH (robusto do Win7 ao Win11, sem admin). Se nao der,
    // usa IOCTL_DISK_PERFORMANCE como reserva (sem % de atividade nesse caso).
    if (GetDiskIoPdh(readMbPerSecond, writeMbPerSecond, activePercent))
    {
        return;
    }
    GetDiskIoIoctl(readMbPerSecond, writeMbPerSecond);
}

SystemSample WindowsMetricsProvider::GetSystemSample()
{
    SystemSample sample;
    sample.timestamp = TimeHelper::NowString();
    sample.cpuPercent = GetCpuPercent();
    GetMemory(sample.memoryTotalMb, sample.memoryUsedMb, sample.memoryUsedPercent);
    GetDisk(sample.diskTotalMb, sample.diskFreeMb, sample.diskUsagePercent);
    GetDiskIo(sample.diskReadMbPerSecond, sample.diskWriteMbPerSecond, sample.diskActivePercent);
    return sample;
}

#endif // _WIN32
