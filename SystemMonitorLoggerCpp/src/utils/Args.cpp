#include "utils/Args.hpp"

#include <cctype>
#include <string>

namespace
{
    std::string ToLower(std::string s)
    {
        for (char& c : s)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }
}

namespace Args
{
    std::optional<long long> ParseDuration(const std::string& text)
    {
        const std::string t = ToLower(text);
        if (t == "notime")
        {
            return -1;
        }
        if (t.empty())
        {
            return std::nullopt;
        }

        long long total = 0;
        long long number = 0;
        bool hasNumber = false;
        bool hasUnit = false;

        for (char c : t)
        {
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                number = number * 10 + (c - '0');
                hasNumber = true;
            }
            else if (c == 'h' || c == 'm' || c == 's')
            {
                if (!hasNumber)
                {
                    return std::nullopt;
                }
                const long long factor = c == 'h' ? 3600 : (c == 'm' ? 60 : 1);
                total += number * factor;
                number = 0;
                hasNumber = false;
                hasUnit = true;
            }
            else
            {
                return std::nullopt;
            }
        }

        // Numero sem unidade no fim e interpretado como minutos (leniente).
        if (hasNumber)
        {
            total += number * 60;
            hasUnit = true;
        }

        return hasUnit ? std::optional<long long>(total) : std::nullopt;
    }

    bool Parse(int argc, char** argv, AppSettings& settings, std::string& error)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--no-smart")
            {
                settings.smartEnabled = false;
            }
            else if (arg == "--simple")
            {
                settings.simpleMode = true;
            }
            else if (arg == "--duration")
            {
                if (i + 1 >= argc)
                {
                    error = "--duration exige um valor (ex.: 30m, 1h, 1h30m, notime).";
                    return false;
                }
                const auto seconds = ParseDuration(argv[++i]);
                if (!seconds.has_value())
                {
                    error = "Duracao invalida. Use 30m, 1h, 1h30m, 90s ou notime.";
                    return false;
                }
                settings.durationSeconds = *seconds;
            }
            else if (arg == "--interval")
            {
                if (i + 1 >= argc)
                {
                    error = "--interval exige um valor em segundos.";
                    return false;
                }
                try
                {
                    const int value = std::stoi(argv[++i]);
                    if (value < 1)
                    {
                        error = "--interval deve ser no minimo 1.";
                        return false;
                    }
                    settings.intervalSeconds = value;
                }
                catch (...)
                {
                    error = "--interval deve ser um numero de segundos.";
                    return false;
                }
            }
            else if (arg == "--help" || arg == "-h")
            {
                error = "help";
                return false;
            }
            else
            {
                error = "Argumento desconhecido: " + arg;
                return false;
            }
        }
        return true;
    }

    std::string HelpText()
    {
        return
            "Uso: SystemMonitorLogger [opcoes]\n"
            "\n"
            "  --duration <valor>  Duracao: 30m, 1h, 1h30m, 90s, notime (padrao: notime)\n"
            "  --interval <n>      Intervalo de coleta em segundos (padrao: 1)\n"
            "  --no-smart          Desativa a verificacao SMART\n"
            "  --simple            Saida textual simples (sem tela ao vivo)\n"
            "  --help, -h          Mostra esta ajuda\n"
            "\n"
            "Sem --duration, roda ate CTRL+C.\n";
    }
}
