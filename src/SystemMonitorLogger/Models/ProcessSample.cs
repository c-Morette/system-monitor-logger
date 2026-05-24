namespace SystemMonitorLogger.Models;

public sealed record ProcessSample(
    DateTimeOffset Timestamp,
    string ProcessName,
    string DisplayName,
    int Pid,
    double CpuPercent,
    double MemoryMb);
