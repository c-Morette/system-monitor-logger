using System.Globalization;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Platform;

public sealed class LinuxMetricsProvider : ISystemMetricsProvider
{
    private CpuTimes? _lastCpuTimes;
    private DiskIoSnapshot? _lastDiskIoSnapshot;

    public async Task<SystemSample> GetSystemSampleAsync(CancellationToken cancellationToken)
    {
        var now = DateTimeOffset.Now;
        var cpuPercent = await GetCpuPercentAsync(cancellationToken);
        var memory = await GetMemoryAsync(cancellationToken);
        var disk = GetPrimaryDisk();
        var diskIo = await GetDiskIoAsync(now, cancellationToken);

        return new SystemSample(
            now,
            cpuPercent,
            memory.UsedPercent,
            memory.UsedMb,
            memory.TotalMb,
            disk.UsedPercent,
            disk.FreeMb,
            disk.TotalMb,
            diskIo.ReadMbPerSecond,
            diskIo.WriteMbPerSecond);
    }

    private async Task<double> GetCpuPercentAsync(CancellationToken cancellationToken)
    {
        string? cpuLine = null;
        await foreach (var line in File.ReadLinesAsync("/proc/stat", cancellationToken))
        {
            cpuLine = line;
            break;
        }

        if (cpuLine is null)
        {
            return 0;
        }

        var firstLine = cpuLine.Split(' ', StringSplitOptions.RemoveEmptyEntries);

        if (firstLine.Length < 8 || firstLine[0] != "cpu")
        {
            return 0;
        }

        var user = Parse(firstLine[1]);
        var nice = Parse(firstLine[2]);
        var system = Parse(firstLine[3]);
        var idle = Parse(firstLine[4]);
        var iowait = Parse(firstLine[5]);
        var irq = Parse(firstLine[6]);
        var softirq = Parse(firstLine[7]);
        var steal = firstLine.Length > 8 ? Parse(firstLine[8]) : 0;

        var current = new CpuTimes(user + nice + system + irq + softirq + steal, idle + iowait);
        if (_lastCpuTimes is null)
        {
            _lastCpuTimes = current;
            return 0;
        }

        var previous = _lastCpuTimes.Value;
        _lastCpuTimes = current;

        var busyDelta = current.Busy - previous.Busy;
        var idleDelta = current.Idle - previous.Idle;
        var total = busyDelta + idleDelta;

        return total <= 0 ? 0 : Math.Clamp(busyDelta * 100d / total, 0, 100);
    }

    private static async Task<MemoryInfo> GetMemoryAsync(CancellationToken cancellationToken)
    {
        var values = new Dictionary<string, double>(StringComparer.OrdinalIgnoreCase);

        await foreach (var line in File.ReadLinesAsync("/proc/meminfo", cancellationToken))
        {
            var parts = line.Split(':', 2);
            if (parts.Length != 2)
            {
                continue;
            }

            var number = parts[1].Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
            if (number is not null && double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out var kb))
            {
                values[parts[0]] = kb;
            }
        }

        var totalMb = values.GetValueOrDefault("MemTotal") / 1024d;
        var availableMb = values.GetValueOrDefault("MemAvailable") / 1024d;
        var usedMb = Math.Max(0, totalMb - availableMb);
        var usedPercent = totalMb <= 0 ? 0 : usedMb * 100d / totalMb;

        return new MemoryInfo(totalMb, usedMb, usedPercent);
    }

    private static DiskInfo GetPrimaryDisk()
    {
        var drive = DriveInfo.GetDrives()
            .Where(d => d.IsReady && d.DriveType == DriveType.Fixed)
            .OrderByDescending(d => d.TotalSize)
            .FirstOrDefault()
            ?? DriveInfo.GetDrives().First(d => d.IsReady);

        var totalMb = drive.TotalSize / 1024d / 1024d;
        var freeMb = drive.AvailableFreeSpace / 1024d / 1024d;
        var usedPercent = totalMb <= 0 ? 0 : (totalMb - freeMb) * 100d / totalMb;

        return new DiskInfo(totalMb, freeMb, usedPercent);
    }

    private async Task<DiskIoInfo> GetDiskIoAsync(DateTimeOffset timestamp, CancellationToken cancellationToken)
    {
        var snapshot = await TryGetDiskIoSnapshotAsync(timestamp, cancellationToken);
        if (snapshot is null)
        {
            return new DiskIoInfo(0, 0);
        }

        if (_lastDiskIoSnapshot is null)
        {
            _lastDiskIoSnapshot = snapshot;
            return new DiskIoInfo(0, 0);
        }

        var previous = _lastDiskIoSnapshot.Value;
        _lastDiskIoSnapshot = snapshot;

        var elapsedSeconds = (snapshot.Value.Timestamp - previous.Timestamp).TotalSeconds;
        if (elapsedSeconds <= 0)
        {
            return new DiskIoInfo(0, 0);
        }

        var readDelta = snapshot.Value.ReadBytes >= previous.ReadBytes ? snapshot.Value.ReadBytes - previous.ReadBytes : 0;
        var writeDelta = snapshot.Value.WriteBytes >= previous.WriteBytes ? snapshot.Value.WriteBytes - previous.WriteBytes : 0;

        return new DiskIoInfo(
            readDelta / 1024d / 1024d / elapsedSeconds,
            writeDelta / 1024d / 1024d / elapsedSeconds);
    }

    private static async Task<DiskIoSnapshot?> TryGetDiskIoSnapshotAsync(DateTimeOffset timestamp, CancellationToken cancellationToken)
    {
        if (!File.Exists("/proc/diskstats"))
        {
            return null;
        }

        ulong readSectors = 0;
        ulong writtenSectors = 0;

        await foreach (var line in File.ReadLinesAsync("/proc/diskstats", cancellationToken))
        {
            var parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 14 || !IsPhysicalDisk(parts[2]))
            {
                continue;
            }

            readSectors += Parse(parts[5]);
            writtenSectors += Parse(parts[9]);
        }

        if (readSectors == 0 && writtenSectors == 0)
        {
            return null;
        }

        const ulong linuxSectorSize = 512;
        return new DiskIoSnapshot(timestamp, readSectors * linuxSectorSize, writtenSectors * linuxSectorSize);
    }

    private static bool IsPhysicalDisk(string name)
    {
        if (name.StartsWith("loop", StringComparison.OrdinalIgnoreCase)
            || name.StartsWith("ram", StringComparison.OrdinalIgnoreCase)
            || name.StartsWith("dm-", StringComparison.OrdinalIgnoreCase)
            || name.StartsWith("md", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        if (name.StartsWith("sd", StringComparison.OrdinalIgnoreCase) && name.Length == 3)
        {
            return true;
        }

        if (name.StartsWith("vd", StringComparison.OrdinalIgnoreCase) && name.Length == 3)
        {
            return true;
        }

        if (name.StartsWith("xvd", StringComparison.OrdinalIgnoreCase) && name.Length == 4)
        {
            return true;
        }

        if (name.StartsWith("nvme", StringComparison.OrdinalIgnoreCase) && name.EndsWith("n1", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        return false;
    }

    private static ulong Parse(string value) => ulong.Parse(value, CultureInfo.InvariantCulture);

    private readonly record struct CpuTimes(ulong Busy, ulong Idle);
    private readonly record struct MemoryInfo(double TotalMb, double UsedMb, double UsedPercent);
    private readonly record struct DiskInfo(double TotalMb, double FreeMb, double UsedPercent);
    private readonly record struct DiskIoInfo(double ReadMbPerSecond, double WriteMbPerSecond);
    private readonly record struct DiskIoSnapshot(DateTimeOffset Timestamp, ulong ReadBytes, ulong WriteBytes);
}
