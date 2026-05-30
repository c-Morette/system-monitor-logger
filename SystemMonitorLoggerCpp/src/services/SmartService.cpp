#include "services/SmartService.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define SML_POPEN _popen
#define SML_PCLOSE _pclose
#else
#include <sys/wait.h>
#define SML_POPEN popen
#define SML_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace
{
    struct CommandResult
    {
        int exitCode = -1;
        std::string output;
    };

    // Executa um comando e captura stdout+stderr (via "2>&1").
    CommandResult RunCommand(const std::string& command)
    {
        CommandResult result;
        FILE* pipe = SML_POPEN((command + " 2>&1").c_str(), "r");
        if (pipe == nullptr)
        {
            return result;
        }

        std::array<char, 4096> buffer{};
        std::size_t read = 0;
        while ((read = std::fread(buffer.data(), 1, buffer.size(), pipe)) > 0)
        {
            result.output.append(buffer.data(), read);
        }

        const int status = SML_PCLOSE(pipe);
#if defined(_WIN32)
        result.exitCode = status;
#else
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
        return result;
    }

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

    std::vector<std::string> SplitWhitespace(const std::string& line)
    {
        std::vector<std::string> parts;
        std::istringstream ss(line);
        std::string token;
        while (ss >> token)
        {
            parts.push_back(token);
        }
        return parts;
    }

    bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle)
    {
        auto it = std::search(
            haystack.begin(), haystack.end(), needle.begin(), needle.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
        return it != haystack.end();
    }

    bool EqualsIgnoreCase(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }
        return true;
    }

    bool SmartctlAvailable()
    {
        const CommandResult r = RunCommand("smartctl --version");
        return r.exitCode == 0 && ContainsIgnoreCase(r.output, "smartctl");
    }

    std::string DiscoverDevice()
    {
        const CommandResult scan = RunCommand("smartctl --scan-open");
        std::istringstream ss(scan.output);
        std::string line;
        while (std::getline(ss, line))
        {
            const std::string trimmed = Trim(line);
            if (trimmed.rfind("/dev/", 0) == 0)
            {
                return SplitWhitespace(trimmed).front();
            }
        }
        return "/dev/sda";
    }

    std::optional<bool> ParseSmartPassed(const std::string& details)
    {
        if (ContainsIgnoreCase(details, "SMART overall-health self-assessment test result: PASSED") ||
            ContainsIgnoreCase(details, "SMART Health Status: OK"))
        {
            return true;
        }
        if (ContainsIgnoreCase(details, "SMART overall-health self-assessment test result: FAILED") ||
            ContainsIgnoreCase(details, "SMART Health Status: FAILED"))
        {
            return false;
        }
        return std::nullopt;
    }

    // Procura uma linha de atributo: "<id> <NAME> ... <raw>" e devolve o ultimo
    // inteiro da linha (raw value). Equivale ao regex do C#.
    std::optional<long long> AttributeValue(const std::string& details,
                                            const std::vector<std::string>& names)
    {
        std::istringstream ss(details);
        std::string line;
        while (std::getline(ss, line))
        {
            const auto parts = SplitWhitespace(line);
            if (parts.size() < 2)
            {
                continue;
            }
            const bool idIsNumber = std::all_of(parts[0].begin(), parts[0].end(),
                [](unsigned char c) { return std::isdigit(c) != 0; });
            if (!idIsNumber)
            {
                continue;
            }
            for (const auto& name : names)
            {
                if (EqualsIgnoreCase(parts[1], name))
                {
                    const std::string& last = parts.back();
                    const bool lastIsNumber = std::all_of(last.begin(), last.end(),
                        [](unsigned char c) { return std::isdigit(c) != 0; });
                    if (lastIsNumber)
                    {
                        try
                        {
                            return std::stoll(last);
                        }
                        catch (...)
                        {
                            return std::nullopt;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    std::optional<long long> TemperatureFallback(const std::string& details)
    {
        std::smatch match;
        const std::regex pattern(R"(Temperature:\s+([0-9]+)\s+Celsius)",
                                 std::regex::icase);
        if (std::regex_search(details, match, pattern))
        {
            try
            {
                return std::stoll(match[1].str());
            }
            catch (...)
            {
            }
        }
        return std::nullopt;
    }

    SmartResult ParseSmartctlOutput(const std::string& device, const std::string& details)
    {
        SmartResult result;
        result.device = device;
        result.details = details;
        result.smartPassed = ParseSmartPassed(details);

        if (result.smartPassed.has_value())
        {
            result.available = true;
            result.status = *result.smartPassed ? "OK" : "FAILED";
        }
        else
        {
            result.available = false;
            result.status = "Nao disponivel ou incompleto";
        }

        auto temperature = AttributeValue(details,
            {"Temperature_Celsius", "Airflow_Temperature_Cel", "Temperature_Internal"});
        if (!temperature.has_value())
        {
            temperature = TemperatureFallback(details);
        }
        if (temperature.has_value())
        {
            result.temperatureCelsius = static_cast<int>(*temperature);
        }

        result.powerOnHours = AttributeValue(details, {"Power_On_Hours", "Power_On_Hours_and_Msec"});
        result.reallocatedSectors = AttributeValue(details, {"Reallocated_Sector_Ct", "Reallocated_Event_Count"});
        result.pendingSectors = AttributeValue(details, {"Current_Pending_Sector", "Offline_Uncorrectable"});
        return result;
    }

    std::string SmartHealthLevel(const SmartResult& r)
    {
        if (r.smartPassed == std::optional<bool>(false) || ContainsIgnoreCase(r.status, "failed"))
        {
            return "CRITICO";
        }
        if (!r.available)
        {
            return "INCOMPLETO";
        }
        if ((r.pendingSectors.value_or(0) > 0) ||
            (r.reallocatedSectors.value_or(0) > 50) ||
            (r.temperatureCelsius.value_or(0) > 55))
        {
            return "ATENCAO";
        }
        return "OK";
    }

    std::string ClassifySectorCount(long long value)
    {
        return value == 0 ? "OK" : "ATENCAO";
    }

    std::string ClassifyTemperature(int value)
    {
        if (value >= 60) return "CRITICO";
        if (value >= 50) return "ATENCAO";
        return "OK";
    }
}

SmartService::SmartService(const std::string& runDirectory)
    : m_runDirectory(runDirectory)
{
}

SmartResult SmartService::Collect()
{
    SmartResult result;

    if (!SmartctlAvailable())
    {
        result = SmartResult::Failed(
            "Motivo: smartctl nao esta disponivel, o aplicativo nao esta elevado, "
            "o disco nao e compativel ou houve erro ao executar smartctl.");
        WriteSmartFile(result);
        return result;
    }

    const std::string device = DiscoverDevice();
    CommandResult run = RunCommand("smartctl -H -A " + device);
    std::string details = Trim(run.output);
    if (details.empty())
    {
        details = "smartctl executou sem retornar dados.";
    }

    result = ParseSmartctlOutput(device, details);
    WriteSmartFile(result);
    return result;
}

void SmartService::WriteSmartFile(const SmartResult& result)
{
    std::ostringstream out;
    out << "Validacao rapida SMART:\n";
    out << "- Saude fisica: " << SmartHealthLevel(result) << "\n";
    out << "- Status informado: " << result.status << "\n";

    if (!result.available)
    {
        out << "- Observacao: SMART nao foi validado completamente. Execute como "
               "administrador/root ou confirme compatibilidade do disco.\n";
    }

    if (result.reallocatedSectors.has_value())
    {
        out << "- Setores realocados: " << *result.reallocatedSectors
            << " (" << ClassifySectorCount(*result.reallocatedSectors) << ")\n";
    }
    if (result.pendingSectors.has_value())
    {
        out << "- Setores pendentes: " << *result.pendingSectors
            << " (" << ClassifySectorCount(*result.pendingSectors) << ")\n";
    }
    if (result.temperatureCelsius.has_value())
    {
        out << "- Temperatura: " << *result.temperatureCelsius
            << " C (" << ClassifyTemperature(*result.temperatureCelsius) << ")\n";
    }

    out << "\nResumo tecnico:\n";
    out << "Status: " << result.status << "\n";
    if (!result.device.empty())
    {
        out << "Dispositivo: " << result.device << "\n";
    }
    if (result.temperatureCelsius.has_value())
    {
        out << "Temperatura: " << *result.temperatureCelsius << " C\n";
    }
    if (result.powerOnHours.has_value())
    {
        out << "Horas de uso: " << *result.powerOnHours << "\n";
    }
    if (result.reallocatedSectors.has_value())
    {
        out << "Setores realocados: " << *result.reallocatedSectors << "\n";
    }
    if (result.pendingSectors.has_value())
    {
        out << "Setores pendentes: " << *result.pendingSectors << "\n";
    }

    out << "\nSaida bruta do smartctl:\n";
    out << result.details << "\n";

    std::ofstream file((fs::path(m_runDirectory) / "smart.txt").string(),
                       std::ios::trunc | std::ios::binary);
    file << out.str();
}
