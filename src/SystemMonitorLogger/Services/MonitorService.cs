using Spectre.Console;
using Spectre.Console.Rendering;
using SystemMonitorLogger.Models;
using SystemMonitorLogger.Platform;

namespace SystemMonitorLogger.Services;

public sealed class MonitorService(
    ISystemMetricsProvider metricsProvider,
    ProcessService processService,
    CsvLogService csvLogService)
{
    private static readonly TimeSpan ProcessSampleInterval = TimeSpan.FromSeconds(5);

    public async Task<MonitorSummary> RunAsync(AppSettings settings, string runDirectory, CancellationToken cancellationToken)
    {
        var summary = new MonitorSummary
        {
            StartedAt = DateTimeOffset.Now
        };

        var nextProcessSampleAt = DateTimeOffset.MinValue;
        using var durationCancellation = settings.Duration is null
            ? null
            : new CancellationTokenSource(settings.Duration.Value);
        using var linkedCancellation = CancellationTokenSource.CreateLinkedTokenSource(
            cancellationToken,
            durationCancellation?.Token ?? CancellationToken.None);

        if (!settings.SimpleMode)
        {
            try
            {
                await AnsiConsole.Live(BuildDashboard(summary, runDirectory, settings))
                    .AutoClear(false)
                    .Overflow(VerticalOverflow.Ellipsis)
                    .Cropping(VerticalOverflowCropping.Bottom)
                    .StartAsync(async context =>
                    {
                        await RunLoopAsync(settings, runDirectory, summary, linkedCancellation, nextProcessSampleAt, context);
                    });
            }
            catch (IOException ex) when (IsConsoleCapabilityError(ex))
            {
                AnsiConsole.MarkupLine("[yellow]Aviso:[/] o terminal atual nao suporta dashboard dinamico. Usando modo simples.");
                var fallbackSettings = settings.WithRuntimeOptions(
                    settings.Duration,
                    settings.IntervalSeconds,
                    settings.EnableSmart,
                    simpleMode: true);
                await RunLoopAsync(fallbackSettings, runDirectory, summary, linkedCancellation, nextProcessSampleAt, null);
            }

            summary.FinishedAt = DateTimeOffset.Now;
            return summary;
        }

        await RunLoopAsync(settings, runDirectory, summary, linkedCancellation, nextProcessSampleAt, null);
        summary.FinishedAt = DateTimeOffset.Now;
        return summary;
    }

    private async Task RunLoopAsync(
        AppSettings settings,
        string runDirectory,
        MonitorSummary summary,
        CancellationTokenSource linkedCancellation,
        DateTimeOffset nextProcessSampleAt,
        LiveDisplayContext? liveContext)
    {
        while (!linkedCancellation.IsCancellationRequested)
        {
            try
            {
                var loopStartedAt = DateTimeOffset.Now;
                var sample = await metricsProvider.GetSystemSampleAsync(linkedCancellation.Token);

                summary.Samples.Add(sample);
                await csvLogService.AppendSystemSampleAsync(sample, CancellationToken.None);

                if (sample.Timestamp >= nextProcessSampleAt)
                {
                    var processSamples = await processService.GetTopProcessesAsync(settings.TopProcessCount, linkedCancellation.Token);
                    summary.ProcessSamples.AddRange(processSamples);
                    await csvLogService.AppendProcessSamplesAsync(processSamples, CancellationToken.None);
                    nextProcessSampleAt = sample.Timestamp.Add(ProcessSampleInterval);
                }

                if (settings.SimpleMode)
                {
                    RenderSimpleStatus(summary);
                }
                else
                {
                    liveContext?.UpdateTarget(BuildDashboard(summary, runDirectory, settings));
                    liveContext?.Refresh();
                }

                var elapsed = DateTimeOffset.Now - loopStartedAt;
                var delay = TimeSpan.FromSeconds(settings.IntervalSeconds) - elapsed;
                if (delay > TimeSpan.Zero)
                {
                    await Task.Delay(delay, linkedCancellation.Token);
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
        }
    }

    private static void RenderSimpleStatus(MonitorSummary summary)
    {
        if (summary.Samples.Count == 0)
        {
            return;
        }

        var last = summary.Samples[^1];
        var elapsed = DateTimeOffset.Now - summary.StartedAt;
        var latestProcessGroup = summary.ProcessSamples
            .GroupBy(p => p.Timestamp)
            .OrderByDescending(g => g.Key)
            .FirstOrDefault();

        var groupedProcesses = latestProcessGroup is null
            ? []
            : GroupProcesses(latestProcessGroup).ToList();

        var topCpuProcess = groupedProcesses
            .OrderByDescending(p => p.CpuPercent)
            .ThenByDescending(p => p.MemoryMb)
            .FirstOrDefault();

        var topRamProcess = groupedProcesses
            .OrderByDescending(p => p.MemoryMb)
            .ThenByDescending(p => p.CpuPercent)
            .FirstOrDefault();

        var processText = topCpuProcess is null || topRamProcess is null
            ? "proc aguardando"
            : $"[grey]C:{Markup.Escape(CompactName(topCpuProcess.DisplayName))}[/] [bold yellow]{topCpuProcess.CpuPercent:0}%[/] [grey]M:{Markup.Escape(CompactName(topRamProcess.DisplayName))}[/] [bold yellow]{topRamProcess.MemoryMb:0}MB[/]";

        AnsiConsole.MarkupLine(
            $"[grey]{DateTime.Now:HH:mm:ss}[/] [yellow]CPU[/]{last.CpuPercent,3:0}% [yellow]RAM[/]{last.MemoryUsedPercent,3:0}% [yellow]DSK[/]{last.DiskUsagePercent,3:0}% [yellow]IO[/]{last.DiskReadMbPerSecond:0.0}/{last.DiskWriteMbPerSecond:0.0} {processText}");
    }

    private static IRenderable BuildDashboard(MonitorSummary summary, string runDirectory, AppSettings settings)
    {
        if (summary.Samples.Count == 0)
        {
            return new Panel("[yellow]Aguardando primeira amostra...[/]")
                .Header("[black on yellow] SystemMonitorLogger [/]")
                .Border(BoxBorder.Rounded)
                .BorderStyle(new Style(Color.Yellow))
                .Expand();
        }

        var last = summary.Samples[^1];
        var elapsed = DateTimeOffset.Now - summary.StartedAt;
        var cpuAvg = summary.Samples.Average(s => s.CpuPercent);
        var cpuPeak = summary.Samples.Max(s => s.CpuPercent);
        var memAvg = summary.Samples.Average(s => s.MemoryUsedPercent);
        var memPeak = summary.Samples.Max(s => s.MemoryUsedPercent);
        var diskAvg = summary.Samples.Average(s => s.DiskUsagePercent);
        var diskPeak = summary.Samples.Max(s => s.DiskUsagePercent);
        var readPeak = summary.Samples.Max(s => s.DiskReadMbPerSecond);
        var writePeak = summary.Samples.Max(s => s.DiskWriteMbPerSecond);

        var metrics = new Table()
            .Border(TableBorder.Rounded)
            .BorderColor(Color.Grey)
            .AddColumn("Metrica")
            .AddColumn(new TableColumn("Atual").RightAligned())
            .AddColumn(new TableColumn("Media").RightAligned())
            .AddColumn(new TableColumn("Pico").RightAligned());

        metrics.AddRow("[yellow]CPU[/]", Percent(last.CpuPercent), Percent(cpuAvg), Percent(cpuPeak));
        metrics.AddRow("[yellow]RAM[/]", Percent(last.MemoryUsedPercent), Percent(memAvg), Percent(memPeak));
        metrics.AddRow("[yellow]Disco usado[/]", Percent(last.DiskUsagePercent), Percent(diskAvg), Percent(diskPeak));
        metrics.AddRow("[grey]Leitura disco[/]", $"{last.DiskReadMbPerSecond:0.0} MB/s", "-", $"{readPeak:0.0} MB/s");
        metrics.AddRow("[grey]Escrita disco[/]", $"{last.DiskWriteMbPerSecond:0.0} MB/s", "-", $"{writePeak:0.0} MB/s");

        var topCpuTable = new Table()
            .Border(TableBorder.Rounded)
            .BorderColor(Color.Grey)
            .AddColumn("#")
            .AddColumn("Processo")
            .AddColumn(new TableColumn("PIDs").RightAligned())
            .AddColumn(new TableColumn("CPU").RightAligned())
            .AddColumn(new TableColumn("RAM").RightAligned());

        var topRamTable = new Table()
            .Border(TableBorder.Rounded)
            .BorderColor(Color.Grey)
            .AddColumn("#")
            .AddColumn("Processo")
            .AddColumn(new TableColumn("PIDs").RightAligned())
            .AddColumn(new TableColumn("RAM").RightAligned())
            .AddColumn(new TableColumn("CPU").RightAligned());

        var latestProcesses = summary.ProcessSamples
            .GroupBy(p => p.Timestamp)
            .OrderByDescending(g => g.Key)
            .FirstOrDefault();

        if (latestProcesses is null)
        {
            topCpuTable.AddRow("-", "Aguardando coleta de processos", "-", "-", "-");
            topRamTable.AddRow("-", "Aguardando coleta de processos", "-", "-", "-");
        }
        else
        {
            var grouped = GroupProcesses(latestProcesses).ToList();
            var index = 1;
            foreach (var process in grouped
                         .OrderByDescending(p => p.CpuPercent)
                         .ThenByDescending(p => p.MemoryMb)
                         .Take(5))
            {
                topCpuTable.AddRow(
                    index.ToString(),
                    Markup.Escape(Trim(process.DisplayName, 30)),
                    process.ProcessCount.ToString(),
                    Principal(Percent(process.CpuPercent)),
                    Secondary($"{process.MemoryMb:0.0} MB"));
                index++;
            }

            index = 1;
            foreach (var process in grouped
                         .OrderByDescending(p => p.MemoryMb)
                         .ThenByDescending(p => p.CpuPercent)
                         .Take(5))
            {
                topRamTable.AddRow(
                    index.ToString(),
                    Markup.Escape(Trim(process.DisplayName, 30)),
                    process.ProcessCount.ToString(),
                    Principal($"{process.MemoryMb:0.0} MB"),
                    Secondary(Percent(process.CpuPercent)));
                index++;
            }
        }

        var header = new Grid()
            .AddColumn()
            .AddColumn();
        header.AddRow("[black on yellow] SystemMonitorLogger [/]", $"[grey]Saida:[/] [yellow]{Markup.Escape(runDirectory)}[/]");
        header.AddRow($"Tempo: [yellow]{elapsed:hh\\:mm\\:ss}[/]   Amostras: [yellow]{summary.Samples.Count}[/]", $"Intervalo: [yellow]{settings.IntervalSeconds}s[/]");
        header.AddRow("[grey]Pressione CTRL+C para finalizar e gerar o relatorio.[/]", string.Empty);

        var processGrid = new Grid()
            .AddColumn()
            .AddColumn();
        processGrid.AddRow(
            new Panel(topCpuTable)
                .Header("[yellow]Top CPU[/]")
                .Border(BoxBorder.Rounded)
                .BorderStyle(new Style(Color.Grey)),
            new Panel(topRamTable)
                .Header("[yellow]Top RAM[/]")
                .Border(BoxBorder.Rounded)
                .BorderStyle(new Style(Color.Grey)));

        var layout = new Rows(
            header,
            metrics,
            processGrid);

        return new Panel(layout)
            .Header("[yellow]Monitorando computador local[/]")
            .Border(BoxBorder.Rounded)
            .BorderStyle(new Style(Color.Yellow))
            .Expand();
    }

    private static string Percent(double value) => $"{value:0.0}%";

    private static string Principal(string value) => $"[bold yellow]{Markup.Escape(value)}[/]";

    private static string Secondary(string value) => $"[grey]{Markup.Escape(value)}[/]";

    private static string Trim(string value, int maxLength)
    {
        return value.Length <= maxLength ? value : value[..Math.Max(0, maxLength - 3)] + "...";
    }

    private static string CompactName(string value)
    {
        return value.ToLowerInvariant() switch
        {
            "visual studio code" => "VS Code",
            "microsoft edge" => "Edge",
            "google chrome" => "Chrome",
            "brave browser" => "Brave",
            "mozilla firefox" => "Firefox",
            "microsoft teams" => "Teams",
            "windows explorer" => "Explorer",
            _ => Trim(value, 8)
        };
    }

    private static IEnumerable<ProcessGroupSample> GroupProcesses(IEnumerable<ProcessSample> samples)
    {
        return samples
            .GroupBy(p => p.DisplayName, StringComparer.OrdinalIgnoreCase)
            .Select(group => new ProcessGroupSample(
                group.Max(p => p.Timestamp),
                group.Select(p => p.ProcessName).GroupBy(name => name, StringComparer.OrdinalIgnoreCase).OrderByDescending(g => g.Count()).First().Key,
                group.Key,
                group.Count(),
                group.Sum(p => p.CpuPercent),
                group.Sum(p => p.MemoryMb)));
    }

    private static bool IsConsoleCapabilityError(IOException ex)
    {
        return ex.Message.Contains("handle", StringComparison.OrdinalIgnoreCase)
            || ex.Message.Contains("identificador", StringComparison.OrdinalIgnoreCase)
            || ex.Message.Contains("console", StringComparison.OrdinalIgnoreCase);
    }
}
