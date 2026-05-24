using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Platform;

public interface ISystemMetricsProvider
{
    Task<SystemSample> GetSystemSampleAsync(CancellationToken cancellationToken);
}
