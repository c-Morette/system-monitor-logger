#pragma once

#include "platform/MetricsProvider.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

// Coleta de metricas no Windows via Win32 API (kernel32). Equivale ao
// WindowsMetricsProvider.cs do projeto C#.
class WindowsMetricsProvider : public MetricsProvider
{
public:
    SystemSample GetSystemSample() override;

private:
    struct CpuTimes
    {
        std::uint64_t idle = 0;
        std::uint64_t kernel = 0;
        std::uint64_t user = 0;
    };

    struct DiskIoSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        std::uint64_t readBytes = 0;
        std::uint64_t writeBytes = 0;
    };

    std::optional<CpuTimes> m_lastCpu;
    std::optional<DiskIoSnapshot> m_lastDiskIo;

    double GetCpuPercent();
    void GetMemory(double& totalMb, double& usedMb, double& usedPercent);
    void GetDisk(double& totalMb, double& freeMb, double& usedPercent);
    void GetDiskIo(double& readMbPerSecond, double& writeMbPerSecond);
};
