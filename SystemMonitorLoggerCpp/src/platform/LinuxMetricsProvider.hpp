#pragma once

#include "platform/MetricsProvider.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

// Coleta de metricas no Linux via /proc e statvfs (sem dependencias externas).
// Equivale ao LinuxMetricsProvider.cs do projeto C#.
class LinuxMetricsProvider : public MetricsProvider
{
public:
    SystemSample GetSystemSample() override;

private:
    // Tempos acumulados de CPU (jiffies). uint64_t: nunca usar long no 32-bit.
    struct CpuTimes
    {
        std::uint64_t busy = 0;
        std::uint64_t idle = 0;
    };

    // Snapshot de I/O de disco para calcular o delta entre amostras.
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
