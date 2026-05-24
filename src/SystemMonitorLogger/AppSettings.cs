using Spectre.Console;

namespace SystemMonitorLogger;

public sealed class AppSettings
{
    public int IntervalSeconds { get; init; } = 1;
    public TimeSpan? Duration { get; init; }
    public string OutputDirectory { get; init; } = "logs";
    public bool EnableSmart { get; init; } = true;
    public bool SimpleMode { get; init; }
    public int TopProcessCount { get; init; } = 5;
    public bool ShowHelp { get; init; }
    public bool InteractiveMode { get; init; }

    public static AppSettings Parse(string[] args)
    {
        var settings = new MutableSettings();

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];

            switch (arg)
            {
                case "--help":
                case "-h":
                    settings.ShowHelp = true;
                    break;
                case "--duration":
                    settings.Duration = ParseOptionalDuration(ReadValue(args, ref i, arg));
                    break;
                case "--interval":
                    settings.IntervalSeconds = ParsePositiveInt(ReadValue(args, ref i, arg), arg);
                    break;
                case "--no-smart":
                    settings.EnableSmart = false;
                    break;
                case "--simple":
                    settings.SimpleMode = true;
                    break;
                default:
                    throw new ArgumentException($"Parametro desconhecido: {arg}");
            }
        }

        return new AppSettings
        {
            IntervalSeconds = settings.IntervalSeconds,
            Duration = settings.Duration,
            OutputDirectory = "logs",
            EnableSmart = settings.EnableSmart,
            SimpleMode = settings.SimpleMode,
            TopProcessCount = settings.TopProcessCount,
            ShowHelp = settings.ShowHelp,
            InteractiveMode = args.Length == 0
        };
    }

    public AppSettings WithRuntimeOptions(TimeSpan? duration, int intervalSeconds, bool enableSmart, bool simpleMode)
    {
        return new AppSettings
        {
            IntervalSeconds = intervalSeconds,
            Duration = duration,
            OutputDirectory = OutputDirectory,
            EnableSmart = enableSmart,
            SimpleMode = simpleMode,
            TopProcessCount = TopProcessCount,
            ShowHelp = ShowHelp,
            InteractiveMode = false
        };
    }

    public static void PrintUsage()
    {
        AnsiConsole.WriteLine("Uso:");
        AnsiConsole.WriteLine("  SystemMonitorLogger [--duration 30m] [--interval 1] [--no-smart] [--simple]");
        AnsiConsole.WriteLine();
        AnsiConsole.WriteLine("Duracao:");
        AnsiConsole.WriteLine("  m = minutos | h = horas | notime");
        AnsiConsole.WriteLine("  Exemplos: 30m | 1h | 1h30m");
        AnsiConsole.WriteLine();
        AnsiConsole.WriteLine("Exemplos:");
        AnsiConsole.WriteLine("  SystemMonitorLogger --duration 5m");
        AnsiConsole.WriteLine("  SystemMonitorLogger --duration 1h30m --interval 1");
        AnsiConsole.WriteLine("  SystemMonitorLogger --no-smart --simple");
    }

    private static string ReadValue(string[] args, ref int index, string option)
    {
        if (index + 1 >= args.Length)
        {
            throw new ArgumentException($"O parametro {option} exige um valor.");
        }

        index++;
        return args[index];
    }

    private static int ParsePositiveInt(string value, string option)
    {
        if (!int.TryParse(value, out var result) || result <= 0)
        {
            throw new ArgumentException($"O parametro {option} deve ser um numero inteiro positivo.");
        }

        return result;
    }

    private static TimeSpan ParseDuration(string value)
    {
        if (value.Length < 2)
        {
            throw new ArgumentException("Duracao invalida. Use m para minutos, h para horas ou combine os dois, como 30m, 1h ou 1h30m.");
        }

        value = value.Trim().ToLowerInvariant();
        var total = TimeSpan.Zero;
        var currentNumber = string.Empty;

        foreach (var character in value)
        {
            if (char.IsDigit(character))
            {
                currentNumber += character;
                continue;
            }

            if (character is not ('s' or 'm' or 'h') || currentNumber.Length == 0)
            {
                throw new ArgumentException("Duracao invalida. Use m para minutos, h para horas ou combine os dois, como 30m, 1h ou 1h30m.");
            }

            var amount = int.Parse(currentNumber);
            if (amount <= 0)
            {
                throw new ArgumentException("Duracao invalida. Informe um valor maior que zero.");
            }

            total += character switch
            {
                's' => TimeSpan.FromSeconds(amount),
                'm' => TimeSpan.FromMinutes(amount),
                _ => TimeSpan.FromHours(amount)
            };
            currentNumber = string.Empty;
        }

        if (currentNumber.Length > 0 || total <= TimeSpan.Zero)
        {
            throw new ArgumentException("Duracao invalida. Use m para minutos, h para horas ou combine os dois, como 30m, 1h ou 1h30m.");
        }

        return total;
    }

    public static TimeSpan? ParseOptionalDuration(string value)
    {
        if (string.IsNullOrWhiteSpace(value)
            || value.Equals("manual", StringComparison.OrdinalIgnoreCase)
                    || value.Equals("notime", StringComparison.OrdinalIgnoreCase)
                    || value.Equals("semtempo", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        return ParseDuration(value.Trim());
    }

    private sealed class MutableSettings
    {
        public int IntervalSeconds { get; set; } = 1;
        public TimeSpan? Duration { get; set; }
        public bool EnableSmart { get; set; } = true;
        public bool SimpleMode { get; set; }
        public bool ShowHelp { get; set; }
        public int TopProcessCount { get; set; } = 5;
    }
}
