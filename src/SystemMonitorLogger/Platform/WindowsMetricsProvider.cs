using System.Runtime.InteropServices;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Platform;

public sealed class WindowsMetricsProvider : ISystemMetricsProvider
{
    private CpuTimes? _lastCpuTimes;
    private DiskIoSnapshot? _lastDiskIoSnapshot;

    public Task<SystemSample> GetSystemSampleAsync(CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();

        var now = DateTimeOffset.Now;
        var cpuPercent = GetCpuPercent();
        var memory = GetMemory();
        var disk = GetPrimaryDisk();
        var diskIo = GetDiskIo(now);

        return Task.FromResult(new SystemSample(
            now,
            cpuPercent,
            memory.UsedPercent,
            memory.UsedMb,
            memory.TotalMb,
            disk.UsedPercent,
            disk.FreeMb,
            disk.TotalMb,
            diskIo.ReadMbPerSecond,
            diskIo.WriteMbPerSecond));
    }

    private double GetCpuPercent()
    {
        if (!GetSystemTimes(out var idleTime, out var kernelTime, out var userTime))
        {
            return 0;
        }

        var current = new CpuTimes(ToUInt64(idleTime), ToUInt64(kernelTime), ToUInt64(userTime));
        if (_lastCpuTimes is null)
        {
            _lastCpuTimes = current;
            return 0;
        }

        var previous = _lastCpuTimes.Value;
        _lastCpuTimes = current;

        var idleDelta = current.Idle - previous.Idle;
        var kernelDelta = current.Kernel - previous.Kernel;
        var userDelta = current.User - previous.User;
        var totalDelta = kernelDelta + userDelta;

        if (totalDelta == 0)
        {
            return 0;
        }

        var busy = totalDelta - idleDelta;
        return Math.Clamp(busy * 100.0 / totalDelta, 0, 100);
    }

    private static MemoryInfo GetMemory()
    {
        var status = new MemoryStatusEx();
        if (!GlobalMemoryStatusEx(status))
        {
            return new MemoryInfo(0, 0, 0);
        }

        var totalMb = status.TotalPhys / 1024d / 1024d;
        var freeMb = status.AvailPhys / 1024d / 1024d;
        var usedMb = Math.Max(0, totalMb - freeMb);
        var usedPercent = totalMb <= 0 ? 0 : usedMb * 100d / totalMb;

        return new MemoryInfo(totalMb, usedMb, usedPercent);
    }

    private static DiskInfo GetPrimaryDisk()
    {
        var systemRoot = Path.GetPathRoot(Environment.SystemDirectory) ?? Path.GetPathRoot(Environment.CurrentDirectory);
        var drive = DriveInfo.GetDrives()
            .Where(d => d.IsReady && string.Equals(d.RootDirectory.FullName, systemRoot, StringComparison.OrdinalIgnoreCase))
            .FirstOrDefault()
            ?? DriveInfo.GetDrives().First(d => d.IsReady);

        var totalMb = drive.TotalSize / 1024d / 1024d;
        var freeMb = drive.AvailableFreeSpace / 1024d / 1024d;
        var usedPercent = totalMb <= 0 ? 0 : (totalMb - freeMb) * 100d / totalMb;

        return new DiskInfo(totalMb, freeMb, usedPercent);
    }

    private DiskIoInfo GetDiskIo(DateTimeOffset timestamp)
    {
        var snapshot = TryGetLogicalDriveIoSnapshot();
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

        var elapsedSeconds = (timestamp - previous.Timestamp).TotalSeconds;
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

    private static DiskIoSnapshot? TryGetLogicalDriveIoSnapshot()
    {
        var root = Path.GetPathRoot(Environment.SystemDirectory);
        if (string.IsNullOrWhiteSpace(root))
        {
            return null;
        }

        var driveLetter = char.ToUpperInvariant(root[0]);
        var path = $@"\\.\{driveLetter}:";
        var handle = CreateFile(path, 0, FileShare.ReadWrite, IntPtr.Zero, FileMode.Open, 0, IntPtr.Zero);
        if (handle == IntPtr.Zero || handle.ToInt64() == -1)
        {
            return null;
        }

        try
        {
            return DeviceIoControl(
                handle,
                IoctlDiskPerformance,
                IntPtr.Zero,
                0,
                out var performance,
                Marshal.SizeOf<DiskPerformance>(),
                out _,
                IntPtr.Zero)
                ? new DiskIoSnapshot(DateTimeOffset.Now, (ulong)performance.BytesRead, (ulong)performance.BytesWritten)
                : null;
        }
        finally
        {
            CloseHandle(handle);
        }
    }

    private static ulong ToUInt64(FileTime fileTime)
    {
        return ((ulong)fileTime.HighDateTime << 32) | (uint)fileTime.LowDateTime;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetSystemTimes(out FileTime idleTime, out FileTime kernelTime, out FileTime userTime);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatusEx lpBuffer);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateFile(
        string fileName,
        uint desiredAccess,
        FileShare shareMode,
        IntPtr securityAttributes,
        FileMode creationDisposition,
        uint flagsAndAttributes,
        IntPtr templateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(
        IntPtr device,
        uint ioControlCode,
        IntPtr inBuffer,
        int inBufferSize,
        out DiskPerformance outBuffer,
        int outBufferSize,
        out int bytesReturned,
        IntPtr overlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    [StructLayout(LayoutKind.Sequential)]
    private struct FileTime
    {
        public uint LowDateTime;
        public uint HighDateTime;
    }

    [StructLayout(LayoutKind.Sequential)]
    private sealed class MemoryStatusEx
    {
        public uint Length = (uint)Marshal.SizeOf<MemoryStatusEx>();
        public uint MemoryLoad;
        public ulong TotalPhys;
        public ulong AvailPhys;
        public ulong TotalPageFile;
        public ulong AvailPageFile;
        public ulong TotalVirtual;
        public ulong AvailVirtual;
        public ulong AvailExtendedVirtual;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DiskPerformance
    {
        public long BytesRead;
        public long BytesWritten;
        public long ReadTime;
        public long WriteTime;
        public long IdleTime;
        public uint ReadCount;
        public uint WriteCount;
        public uint QueueDepth;
        public uint SplitCount;
        public long QueryTime;
        public uint StorageDeviceNumber;
        public char StorageManagerName0;
        public char StorageManagerName1;
        public char StorageManagerName2;
        public char StorageManagerName3;
        public char StorageManagerName4;
        public char StorageManagerName5;
        public char StorageManagerName6;
        public char StorageManagerName7;
    }

    private readonly record struct CpuTimes(ulong Idle, ulong Kernel, ulong User);
    private readonly record struct MemoryInfo(double TotalMb, double UsedMb, double UsedPercent);
    private readonly record struct DiskInfo(double TotalMb, double FreeMb, double UsedPercent);
    private readonly record struct DiskIoInfo(double ReadMbPerSecond, double WriteMbPerSecond);
    private readonly record struct DiskIoSnapshot(DateTimeOffset Timestamp, ulong ReadBytes, ulong WriteBytes);

    private const uint IoctlDiskPerformance = 0x00070020;
}
