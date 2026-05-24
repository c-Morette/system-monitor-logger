namespace SystemMonitorLogger.Models;

public sealed record SystemSample(
    DateTimeOffset Timestamp,
    double CpuPercent,
    double MemoryUsedPercent,
    double MemoryUsedMb,
    double MemoryTotalMb,
    double DiskUsagePercent,
    double DiskFreeMb,
    double DiskTotalMb,
    double DiskReadMbPerSecond,
    double DiskWriteMbPerSecond);
