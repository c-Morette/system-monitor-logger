namespace SystemMonitorLogger.Models;

public sealed class MonitorSummary
{
    public string MachineName { get; init; } = Environment.MachineName;
    public string OperatingSystem { get; init; } = Environment.OSVersion.ToString();
    public DateTimeOffset StartedAt { get; init; }
    public DateTimeOffset FinishedAt { get; set; }
    public List<SystemSample> Samples { get; } = [];
    public List<ProcessSample> ProcessSamples { get; } = [];
    public SmartResult? Smart { get; set; }
}
