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
            else if (arg == "--quiet")
            {
                settings.quietMode = true;
            }
            else if (arg == "--sensitivity")
            {
                if (i + 1 >= argc)
                {
                    error = "--sensitivity exige um valor: baixa, normal ou alta.";
                    return false;
                }
                const std::string preset = ToLower(argv[++i]);
                if (preset == "baixa" || preset == "low")
                {
                    // So picos serios e longos viram incidente (menos ruido).
                    settings.incident = IncidentConfig{60, 90.0, 93.0, 85.0};
                }
                else if (preset == "normal" || preset == "media" || preset == "medium")
                {
                    settings.incident = IncidentConfig{30, 85.0, 90.0, 80.0};
                }
                else if (preset == "alta" || preset == "high")
                {
                    // Pega ate picos curtos/leves (bom para achar problema raro).
                    settings.incident = IncidentConfig{15, 80.0, 85.0, 70.0};
                }
                else
                {
                    error = "Sensibilidade invalida. Use baixa, normal ou alta.";
                    return false;
                }
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
            else if (arg == "--report-every")
            {
                if (i + 1 >= argc)
                {
                    error = "--report-every exige um valor em minutos (0 desliga).";
                    return false;
                }
                try
                {
                    const int minutes = std::stoi(argv[++i]);
                    if (minutes < 0)
                    {
                        error = "--report-every nao pode ser negativo.";
                        return false;
                    }
                    settings.partialReportSeconds = static_cast<long long>(minutes) * 60;
                }
                catch (...)
                {
                    error = "--report-every deve ser um numero de minutos.";
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
            "  --quiet             Modo silencioso: nao desenha nada na tela (so grava logs)\n"
            "  --report-every <n>  Regrava o relatorio a cada n minutos (padrao: 5; 0 desliga)\n"
            "  --sensitivity <p>   Sensibilidade dos incidentes: baixa | normal | alta (padrao: normal)\n"
            "  --help, -h          Mostra esta ajuda\n"
            "\n"
            "Sem --duration, roda ate CTRL+C.\n"
            "Para deixar rodando num PDV (a noite ou em uso): --duration 8h --quiet\n"
            "(o relatorio parcial e regravado a cada 5 min por seguranca).\n";
    }
}
