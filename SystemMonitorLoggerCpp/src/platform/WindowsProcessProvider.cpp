#if defined(_WIN32)

// Baseline Windows 7 (QueryFullProcessImageNameW, PROCESS_QUERY_LIMITED_INFORMATION).
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "platform/WindowsProcessProvider.hpp"
#include "utils/TimeHelper.hpp"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
    std::uint64_t ToU64(const FILETIME& ft)
    {
        ULARGE_INTEGER value;
        value.LowPart = ft.dwLowDateTime;
        value.HighPart = ft.dwHighDateTime;
        return value.QuadPart;
    }

    std::string Narrow(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                             static_cast<int>(wide.size()),
                                             nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                            result.data(), size, nullptr, nullptr);
        return result;
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::string StripExe(std::string name)
    {
        if (name.size() >= 4 && ToLower(name.substr(name.size() - 4)) == ".exe")
        {
            name.erase(name.size() - 4);
        }
        return name;
    }

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

    bool IsUsefulDisplayName(const std::string& value, const std::string& processName)
    {
        if (value.empty())
        {
            return false;
        }
        const std::string v = ToLower(value);
        return v != ToLower(processName) &&
               v != ToLower(processName) + ".exe" &&
               v != "microsoft corporation";
    }

    std::string Normalize(std::string value)
    {
        const std::string mapped = MapKnownProcessName(value);
        if (!mapped.empty())
        {
            return mapped;
        }
        value = StripExe(value);
        if (value == "Code") return "Visual Studio Code";
        if (value == "msedge") return "Microsoft Edge";
        return value;
    }

    // Caminho completo do executavel do processo.
    std::wstring FullImagePath(HANDLE process)
    {
        wchar_t buffer[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, buffer, &size))
        {
            return std::wstring(buffer, size);
        }
        return {};
    }

    // FileDescription dos metadados de versao do executavel.
    std::string FileDescription(const std::wstring& path)
    {
        if (path.empty())
        {
            return {};
        }

        DWORD handle = 0;
        const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
        if (size == 0)
        {
            return {};
        }

        std::vector<BYTE> data(size);
        if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        {
            return {};
        }

        struct LangCodePage
        {
            WORD language;
            WORD codePage;
        };

        LangCodePage* translations = nullptr;
        UINT translationsLen = 0;
        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                            reinterpret_cast<LPVOID*>(&translations), &translationsLen) ||
            translationsLen < sizeof(LangCodePage))
        {
            return {};
        }

        wchar_t subBlock[64];
        swprintf(subBlock, 64, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                 translations[0].language, translations[0].codePage);

        wchar_t* description = nullptr;
        UINT descriptionLen = 0;
        if (VerQueryValueW(data.data(), subBlock,
                           reinterpret_cast<LPVOID*>(&description), &descriptionLen) &&
            descriptionLen > 0)
        {
            return Narrow(std::wstring(description, descriptionLen - 1));
        }
        return {};
    }

    std::string ResolveDisplayName(HANDLE process, const std::string& processName)
    {
        if (process != nullptr)
        {
            const std::string description = FileDescription(FullImagePath(process));
            if (IsUsefulDisplayName(description, processName))
            {
                return Normalize(description);
            }
        }

        const std::string mapped = MapKnownProcessName(processName);
        return mapped.empty() ? processName : mapped;
    }
}

std::vector<ProcessSample> WindowsProcessProvider::CollectAll()
{
    const auto now = std::chrono::steady_clock::now();
    const std::string timestamp = TimeHelper::NowString();

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    const double processors = std::max<DWORD>(1, systemInfo.dwNumberOfProcessors);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    std::vector<ProcessSample> result;
    std::unordered_map<std::int32_t, CpuSnapshot> current;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            const std::int32_t pid = static_cast<std::int32_t>(entry.th32ProcessID);
            const std::string processName = StripExe(Narrow(entry.szExeFile));

            HANDLE process = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);

            std::uint64_t totalTicks = 0;
            double memoryMb = 0.0;
            std::string displayName = processName;

            // Nome amigavel: resolve uma vez por PID e reaproveita (a leitura de
            // version info e' a parte cara; nao faz sentido refazer a cada tick).
            const auto cachedName = m_displayCache.find(pid);
            const bool hasCachedName = cachedName != m_displayCache.end();
            if (hasCachedName)
            {
                displayName = cachedName->second;
            }

            if (process != nullptr)
            {
                FILETIME creation, exit, kernel, user;
                if (GetProcessTimes(process, &creation, &exit, &kernel, &user))
                {
                    totalTicks = ToU64(kernel) + ToU64(user);
                }

                PROCESS_MEMORY_COUNTERS counters;
                if (GetProcessMemoryInfo(process, &counters, sizeof(counters)))
                {
                    memoryMb = static_cast<double>(counters.WorkingSetSize) / 1024.0 / 1024.0;
                }

                if (!hasCachedName)
                {
                    displayName = ResolveDisplayName(process, processName);
                    m_displayCache[pid] = displayName;
                }
                CloseHandle(process);
            }
            else if (!hasCachedName)
            {
                const std::string mapped = MapKnownProcessName(processName);
                displayName = mapped.empty() ? processName : mapped;
            }

            current[pid] = CpuSnapshot{now, totalTicks};

            double cpuPercent = 0.0;
            const auto previous = m_previous.find(pid);
            if (previous != m_previous.end() && totalTicks >= previous->second.totalTicks)
            {
                const double elapsed =
                    std::chrono::duration<double>(now - previous->second.timestamp).count();
                if (elapsed > 0)
                {
                    // ticks sao unidades de 100ns -> segundos = ticks / 1e7.
                    const double cpuSeconds =
                        static_cast<double>(totalTicks - previous->second.totalTicks) / 1e7;
                    cpuPercent = std::clamp(
                        cpuSeconds / elapsed / processors * 100.0, 0.0, 100.0);
                }
            }

            ProcessSample sample;
            sample.timestamp = timestamp;
            sample.processName = processName;
            sample.displayName = displayName;
            sample.pid = pid;
            sample.cpuPercent = cpuPercent;
            sample.memoryMb = memoryMb;
            result.push_back(std::move(sample));

        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    // Remove do cache de nomes os PIDs que nao existem mais (evita crescer
    // indefinidamente e lida com reuso de PID).
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

#endif // _WIN32
