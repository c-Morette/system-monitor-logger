using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Services;

public sealed class SmartService(string runDirectory)
{
    public async Task<SmartResult> CollectAsync(CancellationToken cancellationToken)
    {
        string? extractedPath = null;

        try
        {
            var smartctlPath = await ResolveSmartctlAsync(cancellationToken);
            extractedPath = smartctlPath.ExtractedPath;

            SmartResult result;
            try
            {
                var device = await DiscoverDeviceAsync(smartctlPath.ExecutablePath, cancellationToken);
                result = await RunSmartctlAsync(smartctlPath.ExecutablePath, device, cancellationToken);
            }
            catch (Exception) when (smartctlPath.ExtractedPath is not null)
            {
                var device = await DiscoverDeviceAsync("smartctl", cancellationToken);
                result = await RunSmartctlAsync("smartctl", device, cancellationToken);
            }

            await WriteSmartFileAsync(result, cancellationToken);
            return result;
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            var result = SmartResult.Failed("Motivo: smartctl nao esta disponivel, o aplicativo nao esta elevado, o disco nao e compativel ou houve erro ao executar smartctl. " + ex.Message);
            await WriteSmartFileAsync(result, cancellationToken);
            return result;
        }
        finally
        {
            TryDeleteExtractedSmartctl(extractedPath);
        }
    }

    public Task WriteSmartFileAsync(SmartResult result, CancellationToken cancellationToken)
    {
        var builder = new StringBuilder();
        builder.AppendLine($"Status: {result.Status}");

        if (!string.IsNullOrWhiteSpace(result.Device))
        {
            builder.AppendLine($"Dispositivo: {result.Device}");
        }

        if (result.TemperatureCelsius is not null)
        {
            builder.AppendLine($"Temperatura: {result.TemperatureCelsius} C");
        }

        if (result.PowerOnHours is not null)
        {
            builder.AppendLine($"Horas de uso: {result.PowerOnHours}");
        }

        if (result.ReallocatedSectors is not null)
        {
            builder.AppendLine($"Setores realocados: {result.ReallocatedSectors}");
        }

        if (result.PendingSectors is not null)
        {
            builder.AppendLine($"Setores pendentes: {result.PendingSectors}");
        }

        builder.AppendLine();
        builder.AppendLine(result.Details);

        return File.WriteAllTextAsync(Path.Combine(runDirectory, "smart.txt"), builder.ToString(), Encoding.UTF8, cancellationToken);
    }

    private static async Task<SmartctlPath> ResolveSmartctlAsync(CancellationToken cancellationToken)
    {
        var embedded = await TryExtractEmbeddedSmartctlDirectoryAsync(cancellationToken);
        if (embedded is not null)
        {
            return embedded;
        }

        return new SmartctlPath("smartctl", null);
    }

    private static async Task<SmartctlPath?> TryExtractEmbeddedSmartctlDirectoryAsync(CancellationToken cancellationToken)
    {
        var assembly = Assembly.GetExecutingAssembly();
        var resources = GetSmartctlResourceNames(assembly).ToList();
        if (resources.Count == 0)
        {
            return null;
        }

        var tempDirectory = Path.Combine(Path.GetTempPath(), "SystemMonitorLogger", "smartctl-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDirectory);

        var fileName = OperatingSystem.IsWindows() ? "smartctl.exe" : "smartctl";
        string? executablePath = null;

        foreach (var resourceName in resources)
        {
            var relativePath = GetSmartctlRelativePath(resourceName);
            if (relativePath is null)
            {
                continue;
            }

            var destinationPath = Path.Combine(tempDirectory, relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);

            await using var input = assembly.GetManifestResourceStream(resourceName)
                ?? throw new InvalidOperationException("Recurso smartctl nao encontrado no executavel.");
            await using var output = File.Create(destinationPath);
            await input.CopyToAsync(output, cancellationToken);

            if (Path.GetFileName(destinationPath).Equals(fileName, StringComparison.OrdinalIgnoreCase))
            {
                executablePath = destinationPath;
            }
        }

        if (executablePath is null)
        {
            throw new InvalidOperationException("smartctl foi embutido, mas o executavel esperado nao foi encontrado nos recursos.");
        }

        if (!OperatingSystem.IsWindows())
        {
            await RunProcessAsync("chmod", $"+x \"{executablePath}\"", cancellationToken);
        }

        return new SmartctlPath(executablePath, tempDirectory);
    }

    private static IEnumerable<string> GetSmartctlResourceNames(Assembly assembly)
    {
        var marker = OperatingSystem.IsWindows()
            ? "Resources/smartctl/windows/"
            : "Resources/smartctl/linux/";

        return assembly.GetManifestResourceNames()
            .Where(name => name.StartsWith(marker, StringComparison.OrdinalIgnoreCase));
    }

    private static string? GetSmartctlRelativePath(string resourceName)
    {
        var marker = OperatingSystem.IsWindows()
            ? "Resources/smartctl/windows/"
            : "Resources/smartctl/linux/";

        if (!resourceName.StartsWith(marker, StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        return resourceName[marker.Length..].Replace('/', Path.DirectorySeparatorChar);
    }

    private static async Task<string> DiscoverDeviceAsync(string smartctlPath, CancellationToken cancellationToken)
    {
        var scan = await RunProcessAsync(smartctlPath, "--scan-open", cancellationToken);
        var line = scan.Output
            .Split(Environment.NewLine, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .FirstOrDefault(l => l.StartsWith("/dev/", StringComparison.OrdinalIgnoreCase));

        if (line is not null)
        {
            return line.Split(' ', StringSplitOptions.RemoveEmptyEntries)[0];
        }

        return RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "/dev/sda" : "/dev/sda";
    }

    private static async Task<SmartResult> RunSmartctlAsync(string smartctlPath, string device, CancellationToken cancellationToken)
    {
        var result = await RunProcessAsync(smartctlPath, $"-H -A {device}", cancellationToken);
        var details = string.IsNullOrWhiteSpace(result.Error)
            ? result.Output
            : result.Output + Environment.NewLine + result.Error;

        if (string.IsNullOrWhiteSpace(details))
        {
            details = "smartctl executou sem retornar dados.";
        }

        return ParseSmartctlOutput(device, details.Trim());
    }

    private static SmartResult ParseSmartctlOutput(string device, string details)
    {
        var smartPassed = TryParseSmartPassed(details);
        var status = smartPassed switch
        {
            true => "OK",
            false => "FAILED",
            null => "Nao disponivel ou incompleto"
        };

        var temperature = FirstAttributeValue(details, "Temperature_Celsius", "Airflow_Temperature_Cel", "Temperature_Internal")
            ?? FirstRegexValue(details, @"Temperature:\s+([0-9]+)\s+Celsius");
        var powerOnHours = FirstAttributeValue(details, "Power_On_Hours", "Power_On_Hours_and_Msec");
        var reallocated = FirstAttributeValue(details, "Reallocated_Sector_Ct", "Reallocated_Event_Count");
        var pending = FirstAttributeValue(details, "Current_Pending_Sector", "Offline_Uncorrectable");

        return new SmartResult(
            smartPassed is not null,
            status,
            details,
            device,
            ToInt(temperature),
            powerOnHours,
            reallocated,
            pending,
            smartPassed);
    }

    private static bool? TryParseSmartPassed(string details)
    {
        if (details.Contains("SMART overall-health self-assessment test result: PASSED", StringComparison.OrdinalIgnoreCase)
            || details.Contains("SMART Health Status: OK", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        if (details.Contains("SMART overall-health self-assessment test result: FAILED", StringComparison.OrdinalIgnoreCase)
            || details.Contains("SMART Health Status: FAILED", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return null;
    }

    private static long? FirstAttributeValue(string details, params string[] names)
    {
        foreach (var name in names)
        {
            var match = Regex.Match(
                details,
                $@"^\s*\d+\s+{Regex.Escape(name)}\s+.*?\s+([0-9]+)\s*$",
                RegexOptions.IgnoreCase | RegexOptions.Multiline | RegexOptions.CultureInvariant);
            if (match.Success && long.TryParse(match.Groups[1].Value, out var value))
            {
                return value;
            }
        }

        return null;
    }

    private static long? FirstRegexValue(string details, string pattern)
    {
        var match = Regex.Match(details, pattern, RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        if (match.Success && long.TryParse(match.Groups[1].Value, out var value))
        {
            return value;
        }

        return null;
    }

    private static int? ToInt(long? value)
    {
        return value is null ? null : (int)Math.Clamp(value.Value, int.MinValue, int.MaxValue);
    }

    private static async Task<ProcessResult> RunProcessAsync(string fileName, string arguments, CancellationToken cancellationToken)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        if (Path.IsPathRooted(fileName))
        {
            startInfo.WorkingDirectory = Path.GetDirectoryName(fileName)!;
        }

        using var process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Nao foi possivel iniciar {fileName}.");
        var outputTask = process.StandardOutput.ReadToEndAsync(cancellationToken);
        var errorTask = process.StandardError.ReadToEndAsync(cancellationToken);
        await process.WaitForExitAsync(cancellationToken);

        return new ProcessResult(process.ExitCode, await outputTask, await errorTask);
    }

    private static void TryDeleteExtractedSmartctl(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
            else if (Directory.Exists(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private sealed record SmartctlPath(string ExecutablePath, string? ExtractedPath);
    private sealed record ProcessResult(int ExitCode, string Output, string Error);
}
