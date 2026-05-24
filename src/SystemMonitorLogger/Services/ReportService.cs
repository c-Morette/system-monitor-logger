using System.Globalization;
using System.Text;
using SystemMonitorLogger.Models;

namespace SystemMonitorLogger.Services;

public sealed class ReportService
{
    public async Task<string> GenerateAsync(MonitorSummary summary, string runDirectory, AppSettings settings, CancellationToken cancellationToken)
    {
        var reportPath = Path.Combine(runDirectory, "report.txt");
        var report = BuildReport(summary, settings);
        await File.WriteAllTextAsync(reportPath, report, Encoding.UTF8, cancellationToken);
        return reportPath;
    }

    private static string BuildReport(MonitorSummary summary, AppSettings settings)
    {
        var samples = summary.Samples;
        var diagnosis = Analyze(summary);
        var builder = new StringBuilder();

        builder.AppendLine("SystemMonitorLogger - Relatorio de Diagnostico");
        builder.AppendLine();
        builder.AppendLine($"Computador: {summary.MachineName}");
        builder.AppendLine($"Sistema: {summary.OperatingSystem}");
        builder.AppendLine($"Inicio: {FormatDate(summary.StartedAt)}");
        builder.AppendLine($"Fim: {FormatDate(summary.FinishedAt)}");
        builder.AppendLine($"Duracao: {summary.FinishedAt - summary.StartedAt:hh\\:mm\\:ss}");
        builder.AppendLine($"Intervalo: {settings.IntervalSeconds} segundos");
        builder.AppendLine();

        if (samples.Count == 0)
        {
            builder.AppendLine("Nenhuma amostra foi coletada.");
            return builder.ToString();
        }

        builder.AppendLine("Resumo:");
        AppendMetric(builder, "CPU", samples.Average(s => s.CpuPercent), samples.Max(s => s.CpuPercent));
        AppendMetric(builder, "RAM", samples.Average(s => s.MemoryUsedPercent), samples.Max(s => s.MemoryUsedPercent));
        AppendMetric(builder, "Disco", samples.Average(s => s.DiskUsagePercent), samples.Max(s => s.DiskUsagePercent));
        builder.AppendLine($"Leitura de disco media: {samples.Average(s => s.DiskReadMbPerSecond):0.##} MB/s");
        builder.AppendLine($"Leitura de disco pico: {samples.Max(s => s.DiskReadMbPerSecond):0.##} MB/s");
        builder.AppendLine($"Escrita de disco media: {samples.Average(s => s.DiskWriteMbPerSecond):0.##} MB/s");
        builder.AppendLine($"Escrita de disco pico: {samples.Max(s => s.DiskWriteMbPerSecond):0.##} MB/s");
        builder.AppendLine();
        builder.AppendLine($"Espaco livre em disco: {samples[^1].DiskFreeMb / 1024d:0.##} GB de {samples[^1].DiskTotalMb / 1024d:0.##} GB");
        builder.AppendLine();

        AppendProcessSummary(builder, summary.ProcessSamples);
        AppendSmartSummary(builder, summary.Smart);

        builder.AppendLine();
        builder.AppendLine("Diagnostico provavel:");
        builder.AppendLine($"Possivel gargalo principal: {diagnosis.PrimaryBottleneck}");
        builder.AppendLine(diagnosis.Description);
        builder.AppendLine();
        builder.AppendLine("Evidencias:");
        foreach (var evidence in diagnosis.Evidence)
        {
            builder.AppendLine($"- {evidence}");
        }

        builder.AppendLine();
        builder.AppendLine("Recomendacoes:");

        var item = 1;
        foreach (var recommendation in diagnosis.Recommendations)
        {
            builder.AppendLine($"{item}. {recommendation}");
            item++;
        }

        return builder.ToString();
    }

    private static Diagnosis Analyze(MonitorSummary summary)
    {
        var smart = summary.Smart;
        if (smart is { SmartPassed: false } || smart is { Available: true } && smart.Status.Contains("failed", StringComparison.OrdinalIgnoreCase))
        {
            return new Diagnosis("SMART", "O SMART indicou falha ou risco fisico no armazenamento.", [
                "Fazer backup dos dados importantes.",
                "Verificar a saude do disco com uma ferramenta dedicada.",
                "Considerar substituicao do armazenamento."
            ], BuildSmartEvidence(smart));
        }

        if (smart is { PendingSectors: > 0 } or { ReallocatedSectors: > 50 } or { TemperatureCelsius: > 55 })
        {
            return new Diagnosis("SMART", "O SMART encontrou sinais de alerta no armazenamento.", [
                "Fazer backup dos dados importantes.",
                "Verificar o disco com uma ferramenta dedicada.",
                "Acompanhar se setores pendentes ou realocados aumentam."
            ], BuildSmartEvidence(smart));
        }

        var samples = summary.Samples;
        if (samples.Count == 0)
        {
            return Inconclusive([]);
        }

        var cpuAverage = samples.Average(s => s.CpuPercent);
        var cpuPeak = samples.Max(s => s.CpuPercent);
        var cpuHighSamples = samples.Count(s => s.CpuPercent >= 90);
        var memoryAverage = samples.Average(s => s.MemoryUsedPercent);
        var memoryPeak = samples.Max(s => s.MemoryUsedPercent);
        var memoryHighSamples = samples.Count(s => s.MemoryUsedPercent >= 90);
        var diskAverage = samples.Average(s => s.DiskUsagePercent);
        var diskPeak = samples.Max(s => s.DiskUsagePercent);
        var readAverage = samples.Average(s => s.DiskReadMbPerSecond);
        var readPeak = samples.Max(s => s.DiskReadMbPerSecond);
        var writeAverage = samples.Average(s => s.DiskWriteMbPerSecond);
        var writePeak = samples.Max(s => s.DiskWriteMbPerSecond);
        var diskHighSamples = samples.Count(s => s.DiskUsagePercent >= 95);
        var freePercent = samples[^1].DiskTotalMb <= 0 ? 100 : samples[^1].DiskFreeMb * 100d / samples[^1].DiskTotalMb;

        var cpuScore = Score(cpuAverage, cpuHighSamples, samples.Count, 80, 90);
        var memoryScore = Score(memoryAverage, memoryHighSamples, samples.Count, 85, 90);
        var diskScore = Score(diskAverage, diskHighSamples, samples.Count, 80, 95);

        if (freePercent < 5)
        {
            diskScore += 3;
        }
        else if (freePercent < 15)
        {
            diskScore += 1.5;
        }

        if (readAverage + writeAverage >= 50 || readPeak + writePeak >= 150)
        {
            diskScore += 1.5;
        }

        var evidence = BuildSystemEvidence(summary, cpuAverage, cpuPeak, memoryAverage, memoryPeak, diskAverage, diskPeak, freePercent, readAverage, readPeak, writeAverage, writePeak);
        var best = new[]
        {
            ("CPU", cpuScore),
            ("MEMORIA", memoryScore),
            ("DISCO", diskScore)
        }.OrderByDescending(i => i.Item2).First();

        if (best.Item2 < 2)
        {
            return Inconclusive(evidence);
        }

        return best.Item1 switch
        {
            "CPU" => new Diagnosis("CPU", "A CPU apresentou uso alto durante o monitoramento.", [
                "Verificar os processos com maior consumo.",
                "Repetir o teste durante o momento de lentidao.",
                "Avaliar se ha tarefas em segundo plano consumindo processamento."
            ], evidence),
            "MEMORIA" => new Diagnosis("MEMORIA", "A memoria RAM apresentou uso alto ou pouca folga durante o monitoramento.", [
                "Verificar os processos que mais consomem memoria.",
                "Fechar programas desnecessarios durante o uso do sistema.",
                "Considerar expansao de RAM se o uso alto for constante."
            ], evidence),
            _ => new Diagnosis("DISCO", "O disco apresentou uso elevado ou pouco espaco livre, o que pode causar lentidao.", [
                "Verificar se o computador utiliza HD mecanico.",
                "Liberar espaco em disco se houver pouco espaco livre.",
                "Considerar troca para SSD.",
                "Verificar a saude SMART do disco."
            ], evidence)
        };
    }

    private static List<string> BuildSystemEvidence(
        MonitorSummary summary,
        double cpuAverage,
        double cpuPeak,
        double memoryAverage,
        double memoryPeak,
        double diskAverage,
        double diskPeak,
        double freePercent,
        double readAverage,
        double readPeak,
        double writeAverage,
        double writePeak)
    {
        var evidence = new List<string>
        {
            $"CPU media {cpuAverage:0.##}% e pico {cpuPeak:0.##}%.",
            $"RAM media {memoryAverage:0.##}% e pico {memoryPeak:0.##}%.",
            $"Disco usado medio {diskAverage:0.##}% e pico {diskPeak:0.##}%.",
            $"I/O de disco: leitura media {readAverage:0.##} MB/s, pico {readPeak:0.##} MB/s; escrita media {writeAverage:0.##} MB/s, pico {writePeak:0.##} MB/s.",
            $"Espaco livre em disco {freePercent:0.##}%."
        };

        var groupedProcesses = GroupProcesses(summary.ProcessSamples).ToList();
        var topCpuProcess = groupedProcesses.OrderByDescending(p => p.CpuPercent).FirstOrDefault();
        if (topCpuProcess is not null)
        {
            evidence.Add($"Grupo de processos com maior CPU: {topCpuProcess.DisplayName} ({topCpuProcess.ProcessCount} PIDs) com {topCpuProcess.CpuPercent:0.##}%.");
        }

        var topMemoryProcess = groupedProcesses.OrderByDescending(p => p.MemoryMb).FirstOrDefault();
        if (topMemoryProcess is not null)
        {
            evidence.Add($"Grupo de processos com maior uso de RAM: {topMemoryProcess.DisplayName} ({topMemoryProcess.ProcessCount} PIDs) com {topMemoryProcess.MemoryMb:0.##} MB.");
        }

        return evidence;
    }

    private static List<string> BuildSmartEvidence(SmartResult smart)
    {
        var evidence = new List<string>
        {
            $"Status SMART: {smart.Status}."
        };

        if (!string.IsNullOrWhiteSpace(smart.Device))
        {
            evidence.Add($"Dispositivo analisado: {smart.Device}.");
        }

        if (smart.PendingSectors is not null)
        {
            evidence.Add($"Setores pendentes: {smart.PendingSectors}.");
        }

        if (smart.ReallocatedSectors is not null)
        {
            evidence.Add($"Setores realocados: {smart.ReallocatedSectors}.");
        }

        if (smart.TemperatureCelsius is not null)
        {
            evidence.Add($"Temperatura: {smart.TemperatureCelsius} C.");
        }

        return evidence;
    }

    private static double Score(double average, int highSamples, int totalSamples, double averageThreshold, double highThreshold)
    {
        var score = 0d;
        if (average >= averageThreshold)
        {
            score += 2;
        }

        if (highSamples >= Math.Max(1, totalSamples * 0.2))
        {
            score += highThreshold >= 95 ? 2 : 1.5;
        }

        return score;
    }

    private static Diagnosis Inconclusive(IReadOnlyList<string> evidence)
    {
        return new Diagnosis("INCONCLUSIVO", "Durante o periodo monitorado, nao foram encontrados picos significativos de CPU, memoria ou disco.", [
            "Repetir o teste durante o momento em que a lentidao ocorrer.",
            "Aumentar a duracao do monitoramento.",
            "Comparar os logs gerados em diferentes horarios."
        ], evidence.Count > 0 ? evidence : ["Nenhum limite critico foi atingido durante o periodo monitorado."]);
    }

    private static void AppendMetric(StringBuilder builder, string name, double average, double peak)
    {
        builder.AppendLine($"{name} media: {average.ToString("0.##", CultureInfo.InvariantCulture)}%");
        builder.AppendLine($"{name} pico: {peak.ToString("0.##", CultureInfo.InvariantCulture)}%");
        builder.AppendLine();
    }

    private static void AppendProcessSummary(StringBuilder builder, IReadOnlyList<ProcessSample> processSamples)
    {
        if (processSamples.Count == 0)
        {
            return;
        }

        builder.AppendLine("Processos com maior uso observado por CPU:");

        var grouped = GroupProcesses(processSamples).ToList();
        var topCpu = grouped
            .OrderByDescending(p => p.CpuPercent)
            .ThenByDescending(p => p.MemoryMb)
            .Take(5)
            .ToList();

        foreach (var process in topCpu)
        {
            builder.AppendLine($"- {process.DisplayName} ({process.ProcessCount} PIDs) - CPU: {process.CpuPercent:0.##}% - RAM: {process.MemoryMb:0.##} MB");
        }

        builder.AppendLine();
        builder.AppendLine("Processos com maior uso observado por RAM:");

        var topRam = grouped
            .OrderByDescending(p => p.MemoryMb)
            .ThenByDescending(p => p.CpuPercent)
            .Take(5)
            .ToList();

        foreach (var process in topRam)
        {
            builder.AppendLine($"- {process.DisplayName} ({process.ProcessCount} PIDs) - RAM: {process.MemoryMb:0.##} MB - CPU: {process.CpuPercent:0.##}%");
        }

        builder.AppendLine();
    }

    private static IEnumerable<ProcessGroupSample> GroupProcesses(IEnumerable<ProcessSample> samples)
    {
        var groupedBySample = samples
            .GroupBy(p => p.Timestamp)
            .SelectMany(timestampGroup => timestampGroup
                .GroupBy(p => p.DisplayName, StringComparer.OrdinalIgnoreCase)
                .Select(processGroup => new ProcessGroupSample(
                    timestampGroup.Key,
                    processGroup.Select(p => p.ProcessName).GroupBy(name => name, StringComparer.OrdinalIgnoreCase).OrderByDescending(g => g.Count()).First().Key,
                    processGroup.Key,
                    processGroup.Select(p => p.Pid).Distinct().Count(),
                    processGroup.Sum(p => p.CpuPercent),
                    processGroup.Sum(p => p.MemoryMb))));

        return groupedBySample
            .GroupBy(p => p.DisplayName, StringComparer.OrdinalIgnoreCase)
            .Select(group => new ProcessGroupSample(
                group.Max(p => p.Timestamp),
                group.Select(p => p.ProcessName).GroupBy(name => name, StringComparer.OrdinalIgnoreCase).OrderByDescending(g => g.Count()).First().Key,
                group.Key,
                group.Max(p => p.ProcessCount),
                group.Max(p => p.CpuPercent),
                group.Max(p => p.MemoryMb)));
    }

    private static void AppendSmartSummary(StringBuilder builder, SmartResult? smart)
    {
        builder.AppendLine("SMART:");
        if (smart is null)
        {
            builder.AppendLine("Status: Nao executado");
            return;
        }

        builder.AppendLine($"Status: {smart.Status}");
        if (!string.IsNullOrWhiteSpace(smart.Device))
        {
            builder.AppendLine($"Dispositivo: {smart.Device}");
        }

        if (smart.TemperatureCelsius is not null)
        {
            builder.AppendLine($"Temperatura: {smart.TemperatureCelsius} C");
        }

        if (smart.PowerOnHours is not null)
        {
            builder.AppendLine($"Horas de uso: {smart.PowerOnHours}");
        }

        if (smart.ReallocatedSectors is not null)
        {
            builder.AppendLine($"Setores realocados: {smart.ReallocatedSectors}");
        }

        if (smart.PendingSectors is not null)
        {
            builder.AppendLine($"Setores pendentes: {smart.PendingSectors}");
        }

        builder.AppendLine(smart.Details);
    }

    private static string FormatDate(DateTimeOffset value) => value.ToString("dd/MM/yyyy HH:mm:ss", CultureInfo.InvariantCulture);

    private sealed record Diagnosis(string PrimaryBottleneck, string Description, IReadOnlyList<string> Recommendations, IReadOnlyList<string> Evidence);
}
