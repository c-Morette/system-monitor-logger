namespace SystemMonitorLogger.Models;

public sealed record SmartResult(
    bool Available,
    string Status,
    string Details,
    string? Device = null,
    int? TemperatureCelsius = null,
    long? PowerOnHours = null,
    long? ReallocatedSectors = null,
    long? PendingSectors = null,
    bool? SmartPassed = null)
{
    public static SmartResult Skipped(string reason) => new(false, "Nao executado", reason);
    public static SmartResult Failed(string reason) => new(false, "Nao disponivel ou incompleto", reason);
}
