#if defined(_WIN32)

#include "platform/WindowsMetricsProvider.hpp"
#include "utils/TimeHelper.hpp"

#include <windows.h>
#include <winioctl.h>

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

void WindowsMetricsProvider::GetDiskIo(double& readMbPerSecond, double& writeMbPerSecond)
{
    readMbPerSecond = 0.0;
    writeMbPerSecond = 0.0;

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

SystemSample WindowsMetricsProvider::GetSystemSample()
{
    SystemSample sample;
    sample.timestamp = TimeHelper::NowString();
    sample.cpuPercent = GetCpuPercent();
    GetMemory(sample.memoryTotalMb, sample.memoryUsedMb, sample.memoryUsedPercent);
    GetDisk(sample.diskTotalMb, sample.diskFreeMb, sample.diskUsagePercent);
    GetDiskIo(sample.diskReadMbPerSecond, sample.diskWriteMbPerSecond);
    return sample;
}

#endif // _WIN32
