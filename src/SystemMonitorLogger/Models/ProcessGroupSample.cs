namespace SystemMonitorLogger.Models;

public sealed record ProcessGroupSample(
    DateTimeOffset Timestamp,
    string ProcessName,
    string DisplayName,
    int ProcessCount,
    double CpuPercent,
    double MemoryMb);
