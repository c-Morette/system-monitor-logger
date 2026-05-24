using System.Runtime.InteropServices;
using Spectre.Console;
using SystemMonitorLogger.Models;
using SystemMonitorLogger.Platform;
using SystemMonitorLogger.Services;
using SystemMonitorLogger.Utils;

namespace SystemMonitorLogger;

internal static class Program
{
    private static async Task<int> Main(string[] args)
    {
        AppSettings settings;

        try
        {
            settings = AppSettings.Parse(args);
        }
        catch (ArgumentException ex)
        {
            AnsiConsole.MarkupLine($"[red]Erro nos argumentos:[/] {Markup.Escape(ex.Message)}");
            AppSettings.PrintUsage();
            return 1;
        }

        if (settings.ShowHelp)
        {
            AppSettings.PrintUsage();
            return 0;
        }

        if (settings.InteractiveMode)
        {
            try
            {
                settings = InteractiveSettingsService.Ask(settings);
            }
            catch (OperationCanceledException)
            {
                AnsiConsole.MarkupLine("[yellow]Monitoramento cancelado.[/]");
                return 0;
            }
        }

        var runDirectory = FileHelper.CreateRunDirectory(settings.OutputDirectory);
        var cancellation = new CancellationTokenSource();

        Console.CancelKeyPress += (_, eventArgs) =>
        {
            eventArgs.Cancel = true;
            cancellation.Cancel();
        };

        if (!AdminHelper.IsElevated())
        {
            AnsiConsole.MarkupLine("[yellow]Aviso:[/] o aplicativo nao esta rodando como administrador/root.");
            AnsiConsole.MarkupLine("[grey]A verificacao da saude fisica do disco pode ficar indisponivel.[/]");
        }

        var provider = CreateMetricsProvider();
        var processService = new ProcessService();
        var csvLogService = new CsvLogService(runDirectory);
        var reportService = new ReportService();
        var smartService = new SmartService(runDirectory);
        var monitorService = new MonitorService(provider, processService, csvLogService);

        AnsiConsole.MarkupLine("[black on yellow] SystemMonitorLogger [/]");
        AnsiConsole.MarkupLine("Monitorando computador local...");
        AnsiConsole.WriteLine();

        SmartResult? smartResult = null;
        if (settings.EnableSmart)
        {
            smartResult = await smartService.CollectAsync(cancellation.Token);
        }
        else
        {
            smartResult = SmartResult.Skipped("SMART desativado por parametro --no-smart.");
            await smartService.WriteSmartFileAsync(smartResult, cancellation.Token);
        }

        var summary = await monitorService.RunAsync(settings, runDirectory, cancellation.Token);
        summary.Smart = smartResult;

        var reportPath = await reportService.GenerateAsync(summary, runDirectory, settings, CancellationToken.None);

        AnsiConsole.WriteLine();
        AnsiConsole.MarkupLine("[green]Monitoramento finalizado.[/]");
        AnsiConsole.MarkupLine("Relatorio gerado em:");
        AnsiConsole.WriteLine(reportPath);

        return 0;
    }

    private static ISystemMetricsProvider CreateMetricsProvider()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            return new WindowsMetricsProvider();
        }

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            return new LinuxMetricsProvider();
        }

        throw new PlatformNotSupportedException("Sistema operacional nao suportado. Use Windows 10/11 ou Linux/Lubuntu.");
    }
}
