using System.Diagnostics;
using System.Runtime.InteropServices;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Services;

public sealed class ProcessService
{
    private readonly Dictionary<int, ProcessCpuSnapshot> _previousCpuSnapshots = [];

    public Task<List<ProcessSample>> GetTopProcessesAsync(int count, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();

        var timestamp = DateTimeOffset.Now;
        var processorCount = Math.Max(1, Environment.ProcessorCount);
        var samples = new List<ProcessSample>();
        var currentSnapshots = new Dictionary<int, ProcessCpuSnapshot>();

        foreach (var process in Process.GetProcesses())
        {
            using (process)
            {
                try
                {
                    var totalProcessorTime = process.TotalProcessorTime;
                    currentSnapshots[process.Id] = new ProcessCpuSnapshot(timestamp, totalProcessorTime);

                    var cpuPercent = 0d;
                    if (_previousCpuSnapshots.TryGetValue(process.Id, out var previous))
                    {
                        var elapsedMs = (timestamp - previous.Timestamp).TotalMilliseconds;
                        var cpuMs = (totalProcessorTime - previous.TotalProcessorTime).TotalMilliseconds;

                        if (elapsedMs > 0 && cpuMs >= 0)
                        {
                            cpuPercent = Math.Clamp(cpuMs / elapsedMs / processorCount * 100d, 0, 100);
                        }
                    }

                    samples.Add(new ProcessSample(
                        timestamp,
                        process.ProcessName,
                        GetDisplayName(process),
                        process.Id,
                        cpuPercent,
                        process.WorkingSet64 / 1024d / 1024d));
                }
                catch (InvalidOperationException)
                {
                }
                catch (System.ComponentModel.Win32Exception)
                {
                }
            }
        }

        _previousCpuSnapshots.Clear();
        foreach (var snapshot in currentSnapshots)
        {
            _previousCpuSnapshots[snapshot.Key] = snapshot.Value;
        }

        var topByMemory = samples
            .OrderByDescending(p => p.MemoryMb)
            .Take(count);

        var topByCpu = samples
            .OrderByDescending(p => p.CpuPercent)
            .ThenByDescending(p => p.MemoryMb)
            .Take(count);

        var result = topByMemory
            .Concat(topByCpu)
            .GroupBy(p => p.Pid)
            .Select(g => g.First())
            .Take(count * 2)
            .ToList();

        return Task.FromResult(result);
    }

    private static string GetDisplayName(Process process)
    {
        var metadataName = TryGetMetadataDisplayName(process);
        if (!string.IsNullOrWhiteSpace(metadataName))
        {
            return NormalizeDisplayName(metadataName);
        }

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            var linuxName = TryGetLinuxCommandDisplayName(process.Id);
            if (!string.IsNullOrWhiteSpace(linuxName))
            {
                return NormalizeDisplayName(linuxName);
            }
        }

        return MapKnownProcessName(process.ProcessName) ?? process.ProcessName;
    }

    private static string? TryGetMetadataDisplayName(Process process)
    {
        try
        {
            var versionInfo = process.MainModule?.FileVersionInfo;
            var fileDescription = versionInfo?.FileDescription;
            if (IsUsefulDisplayName(fileDescription, process.ProcessName))
            {
                return fileDescription;
            }

            var productName = versionInfo?.ProductName;
            if (IsUsefulDisplayName(productName, process.ProcessName))
            {
                return productName;
            }
        }
        catch (InvalidOperationException)
        {
        }
        catch (System.ComponentModel.Win32Exception)
        {
        }
        catch (NotSupportedException)
        {
        }

        return null;
    }

    private static string? TryGetLinuxCommandDisplayName(int pid)
    {
        try
        {
            var cmdlinePath = $"/proc/{pid}/cmdline";
            if (!File.Exists(cmdlinePath))
            {
                return null;
            }

            var content = File.ReadAllText(cmdlinePath).Replace('\0', ' ').Trim();
            if (string.IsNullOrWhiteSpace(content))
            {
                return null;
            }

            var executable = content.Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
            if (string.IsNullOrWhiteSpace(executable))
            {
                return null;
            }

            return Path.GetFileNameWithoutExtension(executable);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }

        return null;
    }

    private static bool IsUsefulDisplayName(string? value, string processName)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var trimmed = value.Trim();
        if (trimmed.Equals(processName, StringComparison.OrdinalIgnoreCase)
            || trimmed.Equals(processName + ".exe", StringComparison.OrdinalIgnoreCase)
            || trimmed.Equals("Microsoft Corporation", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return true;
    }

    private static string NormalizeDisplayName(string value)
    {
        var trimmed = value.Trim();
        var mapped = MapKnownProcessName(trimmed);
        if (mapped is not null)
        {
            return mapped;
        }

        if (trimmed.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
        {
            trimmed = trimmed[..^4];
        }

        return trimmed switch
        {
            "Code" => "Visual Studio Code",
            "msedge" => "Microsoft Edge",
            _ => trimmed
        };
    }

    private static string? MapKnownProcessName(string processName)
    {
        return processName.ToLowerInvariant() switch
        {
            "code" => "Visual Studio Code",
            "msedge" => "Microsoft Edge",
            "chrome" => "Google Chrome",
            "brave" => "Brave Browser",
            "firefox" => "Mozilla Firefox",
            "devenv" => "Visual Studio",
            "discord" => "Discord",
            "explorer" => "Windows Explorer",
            "teams" or "msteams" => "Microsoft Teams",
            _ => null
        };
    }

    private sealed record ProcessCpuSnapshot(DateTimeOffset Timestamp, TimeSpan TotalProcessorTime);
}
