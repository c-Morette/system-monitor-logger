using System.Globalization;
using System.Text;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Services;

public sealed class CsvLogService
{
    private readonly string _samplesPath;
    private readonly string _processesPath;

    public CsvLogService(string runDirectory)
    {
        _samplesPath = Path.Combine(runDirectory, "samples.csv");
        _processesPath = Path.Combine(runDirectory, "processes.csv");

        File.WriteAllText(_samplesPath, "timestamp,cpu_percent,memory_used_percent,memory_used_mb,memory_total_mb,disk_usage_percent,disk_free_mb,disk_total_mb,disk_read_mb_s,disk_write_mb_s" + Environment.NewLine, Encoding.UTF8);
        File.WriteAllText(_processesPath, "timestamp,process_name,display_name,pid,cpu_percent,memory_mb" + Environment.NewLine, Encoding.UTF8);
    }

    public async Task AppendSystemSampleAsync(SystemSample sample, CancellationToken cancellationToken)
    {
        var line = string.Join(',',
            sample.Timestamp.ToString("O", CultureInfo.InvariantCulture),
            Format(sample.CpuPercent),
            Format(sample.MemoryUsedPercent),
            Format(sample.MemoryUsedMb),
            Format(sample.MemoryTotalMb),
            Format(sample.DiskUsagePercent),
            Format(sample.DiskFreeMb),
            Format(sample.DiskTotalMb),
            Format(sample.DiskReadMbPerSecond),
            Format(sample.DiskWriteMbPerSecond));

        await File.AppendAllTextAsync(_samplesPath, line + Environment.NewLine, Encoding.UTF8, cancellationToken);
    }

    public async Task AppendProcessSamplesAsync(IEnumerable<ProcessSample> samples, CancellationToken cancellationToken)
    {
        var builder = new StringBuilder();

        foreach (var sample in samples)
        {
            builder.AppendLine(string.Join(',',
                sample.Timestamp.ToString("O", CultureInfo.InvariantCulture),
                Escape(sample.ProcessName),
                Escape(sample.DisplayName),
                sample.Pid.ToString(CultureInfo.InvariantCulture),
                Format(sample.CpuPercent),
                Format(sample.MemoryMb)));
        }

        if (builder.Length > 0)
        {
            await File.AppendAllTextAsync(_processesPath, builder.ToString(), Encoding.UTF8, cancellationToken);
        }
    }

    private static string Format(double value) => value.ToString("0.##", CultureInfo.InvariantCulture);

    private static string Escape(string value)
    {
        if (!value.Contains(',') && !value.Contains('"') && !value.Contains('\n') && !value.Contains('\r'))
        {
            return value;
        }

        return '"' + value.Replace("\"", "\"\"", StringComparison.Ordinal) + '"';
    }
}
