#if !defined(_WIN32)

#include "platform/LinuxProcessProvider.hpp"
#include "utils/TimeHelper.hpp"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    std::string ReadFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    bool AllDigits(const std::string& s)
    {
        if (s.empty())
        {
            return false;
        }
        return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Nomes amigaveis conhecidos (mesma tabela do projeto C#).
    std::string MapKnownProcessName(const std::string& processName)
    {
        const std::string p = ToLower(processName);
        if (p == "code") return "Visual Studio Code";
        if (p == "msedge") return "Microsoft Edge";
        if (p == "chrome") return "Google Chrome";
        if (p == "brave") return "Brave Browser";
        if (p == "firefox") return "Mozilla Firefox";
        if (p == "devenv") return "Visual Studio";
        if (p == "discord") return "Discord";
        if (p == "explorer") return "Windows Explorer";
        if (p == "teams" || p == "msteams") return "Microsoft Teams";
        return {};
    }

    // Nome do executavel a partir de /proc/<pid>/cmdline (1o argumento, basename).
    std::string DisplayFromCmdline(std::int32_t pid)
    {
        std::string content = ReadFile("/proc/" + std::to_string(pid) + "/cmdline");
        if (content.empty())
        {
            return {};
        }
        // cmdline e separado por '\0'; pega o primeiro argumento.
        const auto nul = content.find('\0');
        std::string first = nul == std::string::npos ? content : content.substr(0, nul);
        if (first.empty())
        {
            return {};
        }
        return fs::path(first).filename().string();
    }

    std::string ResolveDisplayName(std::int32_t pid, const std::string& processName)
    {
        const std::string cmd = DisplayFromCmdline(pid);
        const std::string candidate = cmd.empty() ? processName : cmd;

        const std::string mapped = MapKnownProcessName(candidate);
        if (!mapped.empty())
        {
            return mapped;
        }
        return candidate;
    }

    // Le comm e (utime+stime) de /proc/<pid>/stat. O campo comm vem entre
    // parenteses e pode conter espacos, entao usamos o ultimo ')'.
    bool ReadStat(std::int32_t pid, std::string& comm, std::uint64_t& totalTicks)
    {
        const std::string stat = ReadFile("/proc/" + std::to_string(pid) + "/stat");
        if (stat.empty())
        {
            return false;
        }

        const auto open = stat.find('(');
        const auto close = stat.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open)
        {
            return false;
        }

        comm = stat.substr(open + 1, close - open - 1);

        // Campos apos ')': indice 0 = state (campo 3). utime = campo 14 (indice 11),
        // stime = campo 15 (indice 12).
        std::istringstream rest(stat.substr(close + 2));
        std::vector<std::string> fields;
        std::string token;
        while (rest >> token)
        {
            fields.push_back(token);
        }
        if (fields.size() < 13)
        {
            return false;
        }

        const std::uint64_t utime = std::strtoull(fields[11].c_str(), nullptr, 10);
        const std::uint64_t stime = std::strtoull(fields[12].c_str(), nullptr, 10);
        totalTicks = utime + stime;
        return true;
    }

    // Memoria residente (RSS) a partir de /proc/<pid>/statm (2o campo, em paginas).
    double ReadMemoryMb(std::int32_t pid, long pageSize)
    {
        const std::string statm = ReadFile("/proc/" + std::to_string(pid) + "/statm");
        if (statm.empty())
        {
            return 0.0;
        }
        std::istringstream ss(statm);
        std::uint64_t totalPages = 0, residentPages = 0;
        ss >> totalPages >> residentPages;
        return static_cast<double>(residentPages) * static_cast<double>(pageSize) / 1024.0 / 1024.0;
    }
}

std::vector<ProcessSample> LinuxProcessProvider::CollectAll()
{
    const auto now = std::chrono::steady_clock::now();
    const std::string timestamp = TimeHelper::NowString();
    const long clockTicks = sysconf(_SC_CLK_TCK);
    const long pageSize = sysconf(_SC_PAGESIZE);
    const long processors = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));

    std::vector<ProcessSample> result;
    std::unordered_map<std::int32_t, CpuSnapshot> current;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec))
    {
        if (ec)
        {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (!AllDigits(name))
        {
            continue;
        }

        const std::int32_t pid = static_cast<std::int32_t>(std::strtol(name.c_str(), nullptr, 10));

        std::string comm;
        std::uint64_t totalTicks = 0;
        if (!ReadStat(pid, comm, totalTicks))
        {
            continue;
        }

        current[pid] = CpuSnapshot{now, totalTicks};

        double cpuPercent = 0.0;
        const auto previous = m_previous.find(pid);
        if (previous != m_previous.end() && clockTicks > 0)
        {
            const double elapsed =
                std::chrono::duration<double>(now - previous->second.timestamp).count();
            if (elapsed > 0 && totalTicks >= previous->second.totalTicks)
            {
                const double cpuSeconds =
                    static_cast<double>(totalTicks - previous->second.totalTicks) /
                    static_cast<double>(clockTicks);
                cpuPercent = std::clamp(
                    cpuSeconds / elapsed / static_cast<double>(processors) * 100.0, 0.0, 100.0);
            }
        }

        // Nome amigavel: resolve uma vez por PID e reaproveita.
        std::string displayName;
        const auto cachedName = m_displayCache.find(pid);
        if (cachedName != m_displayCache.end())
        {
            displayName = cachedName->second;
        }
        else
        {
            displayName = ResolveDisplayName(pid, comm);
            m_displayCache[pid] = displayName;
        }

        ProcessSample sample;
        sample.timestamp = timestamp;
        sample.processName = comm;
        sample.displayName = displayName;
        sample.pid = pid;
        sample.cpuPercent = cpuPercent;
        sample.memoryMb = ReadMemoryMb(pid, pageSize);
        result.push_back(std::move(sample));
    }

    // Remove do cache de nomes os PIDs que nao existem mais.
    for (auto it = m_displayCache.begin(); it != m_displayCache.end();)
    {
        if (current.count(it->first) == 0)
        {
            it = m_displayCache.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_previous = std::move(current);
    return result;
}

#endif // !_WIN32
