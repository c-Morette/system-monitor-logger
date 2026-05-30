#include "services/InteractiveSettings.hpp"
#include "utils/Args.hpp"
#include "utils/TimeHelper.hpp"

#include <cctype>
#include <iostream>
#include <string>

namespace
{
    std::string Trim(const std::string& s)
    {
        const auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return {};
        }
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    std::string ToLower(std::string s)
    {
        for (char& c : s)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }

    // Le uma linha; vazio (ou sem stdin) devolve o valor padrao.
    std::string AskLine(const std::string& prompt, const std::string& def)
    {
        std::cout << prompt;
        if (!def.empty())
        {
            std::cout << " [" << def << "]";
        }
        std::cout << ": " << std::flush;

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return def;
        }
        line = Trim(line);
        return line.empty() ? def : line;
    }

    bool AskYesNo(const std::string& prompt, bool defaultYes)
    {
        const std::string suffix = defaultYes ? " (S/n)" : " (s/N)";
        const std::string def = defaultYes ? "s" : "n";
        while (true)
        {
            const std::string r = ToLower(AskLine(prompt + suffix, def));
            if (r == "s" || r == "sim" || r == "y" || r == "yes")
            {
                return true;
            }
            if (r == "n" || r == "nao" || r == "no")
            {
                return false;
            }
            std::cout << "Responda s ou n.\n";
        }
    }
}

namespace InteractiveSettings
{
    bool Ask(AppSettings& settings)
    {
        std::cout << "============================================\n";
        std::cout << " SystemMonitorLogger - configuracao\n";
        std::cout << "============================================\n";
        std::cout << "Pressione ENTER para aceitar o valor entre [colchetes].\n";
        std::cout << "Os logs sao salvos automaticamente em ./logs.\n\n";

        // Duracao
        std::cout << "Duracao  (m = minutos, h = horas, notime = sem limite)\n";
        std::cout << "Exemplos: 30m | 1h | 1h30m | notime\n";
        while (true)
        {
            const std::string text = AskLine("Tempo de monitoramento", "notime");
            const auto seconds = Args::ParseDuration(text);
            if (seconds.has_value())
            {
                settings.durationSeconds = *seconds;
                break;
            }
            std::cout << "Duracao invalida. Use 30m, 1h, 1h30m, 90s ou notime.\n";
        }

        // Intervalo
        while (true)
        {
            const std::string text = AskLine("Intervalo de atualizacao em segundos",
                                             std::to_string(settings.intervalSeconds));
            try
            {
                const int value = std::stoi(text);
                if (value > 0)
                {
                    settings.intervalSeconds = value;
                    break;
                }
            }
            catch (...)
            {
            }
            std::cout << "Informe um numero maior que zero.\n";
        }

        settings.smartEnabled = AskYesNo("Verificar a saude fisica do disco (SMART)?", settings.smartEnabled);
        settings.simpleMode = AskYesNo("Usar modo simples, sem tela dinamica?", settings.simpleMode);

        // Resumo
        const std::string duration = settings.durationSeconds < 0
            ? std::string("ate CTRL+C")
            : TimeHelper::FormatDuration(settings.durationSeconds);

        std::cout << "\n----------------- Resumo -----------------\n";
        std::cout << "Duracao:        " << duration << "\n";
        std::cout << "Intervalo:      " << settings.intervalSeconds << "s\n";
        std::cout << "Saude do disco: " << (settings.smartEnabled ? "verificar quando possivel" : "nao verificar") << "\n";
        std::cout << "Modo:           " << (settings.simpleMode ? "simples" : "dashboard") << "\n";
        std::cout << "Saida:          ./logs\n";
        std::cout << "------------------------------------------\n\n";

        return AskYesNo("Iniciar monitoramento agora?", true);
    }
}
