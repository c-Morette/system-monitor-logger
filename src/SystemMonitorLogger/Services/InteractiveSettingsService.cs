using Spectre.Console;

namespace SystemMonitorLogger.Services;

public static class InteractiveSettingsService
{
    public static AppSettings Ask(AppSettings defaults)
    {
        AnsiConsole.Clear();
        AnsiConsole.Write(
            new FigletText("SystemMonitorLogger")
                .LeftJustified()
                .Color(Color.Yellow));

        AnsiConsole.MarkupLine("[grey]Configure o monitoramento antes de iniciar. Os logs serao salvos automaticamente em ./logs.[/]");
        AnsiConsole.WriteLine();

        AnsiConsole.MarkupLine("[yellow]Duracao[/]");
        AnsiConsole.MarkupLine("[grey]m = minutos | h = horas | notime[/]");
        AnsiConsole.MarkupLine("[grey]Exemplos: 30m | 1h | 1h30m[/]");

        var durationText = AnsiConsole.Prompt(
            new TextPrompt<string>("Tempo de monitoramento")
                .DefaultValue("notime")
                .Validate(value =>
                {
                    try
                    {
                        AppSettings.ParseOptionalDuration(value);
                        return ValidationResult.Success();
                    }
                    catch (ArgumentException ex)
                    {
                        return ValidationResult.Error(ex.Message);
                    }
                }));

        var interval = AnsiConsole.Prompt(
            new TextPrompt<int>("Intervalo de atualizacao em segundos")
                .DefaultValue(defaults.IntervalSeconds)
                .Validate(value => value > 0
                    ? ValidationResult.Success()
                    : ValidationResult.Error("Informe um numero maior que zero.")));

        var enableSmart = AnsiConsole.Confirm("Verificar a saude fisica do disco quando possivel?", defaults.EnableSmart);
        var simpleMode = AnsiConsole.Confirm("Usar modo simples, sem tela dinamica?", defaults.SimpleMode);

        var duration = AppSettings.ParseOptionalDuration(durationText);

        AnsiConsole.WriteLine();
        AnsiConsole.Write(new Rule("[yellow]Resumo[/]").RuleStyle("grey"));
        AnsiConsole.MarkupLine($"Duracao: [yellow]{(duration is null ? "ate CTRL+C" : FormatDuration(duration.Value))}[/]");
        AnsiConsole.MarkupLine($"Intervalo: [yellow]{interval}s[/]");
        AnsiConsole.MarkupLine($"Saude do disco: [yellow]{(enableSmart ? "verificar quando possivel" : "nao verificar")}[/]");
        AnsiConsole.MarkupLine($"Modo: [yellow]{(simpleMode ? "simples" : "dashboard")}[/]");
        AnsiConsole.MarkupLine("Saida: [yellow]./logs[/]");
        AnsiConsole.WriteLine();

        if (!AnsiConsole.Confirm("Iniciar monitoramento agora?", true))
        {
            throw new OperationCanceledException("Monitoramento cancelado pelo usuario.");
        }

        return defaults.WithRuntimeOptions(duration, interval, enableSmart, simpleMode);
    }

    private static string FormatDuration(TimeSpan duration)
    {
        if (duration.TotalHours >= 1)
        {
            return $"{(int)duration.TotalHours}h {duration.Minutes}m";
        }

        return $"{duration.Minutes}m";
    }
}
