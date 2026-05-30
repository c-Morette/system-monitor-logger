#include "utils/FileHelper.hpp"
#include "utils/TimeHelper.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
    // Troca caracteres invalidos em nome de arquivo por '-'.
    std::string MakeSafeFileName(const std::string& value)
    {
        std::string result = value;
        const std::string invalid = "<>:\"/\\|?*";
        for (char& c : result)
        {
            if (invalid.find(c) != std::string::npos ||
                static_cast<unsigned char>(c) < 32)
            {
                c = '-';
            }
        }
        return result;
    }

    // Pasta onde o executavel esta (para salvar logs ao lado dele).
    fs::path ExecutableDirectory()
    {
#if defined(_WIN32)
        char buffer[MAX_PATH];
        const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length == 0)
        {
            return fs::current_path();
        }
        return fs::path(std::string(buffer, length)).parent_path();
#else
        char buffer[PATH_MAX];
        const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (length <= 0)
        {
            return fs::current_path();
        }
        buffer[length] = '\0';
        return fs::path(buffer).parent_path();
#endif
    }
}

namespace FileHelper
{
    std::string MachineName()
    {
#if defined(_WIN32)
        char buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buffer);
        if (GetComputerNameA(buffer, &size))
        {
            return std::string(buffer, size);
        }
        return "desconhecido";
#else
        char buffer[256];
        if (gethostname(buffer, sizeof(buffer)) == 0)
        {
            buffer[sizeof(buffer) - 1] = '\0';
            return std::string(buffer);
        }
        return "desconhecido";
#endif
    }

    long ProcessId()
    {
#if defined(_WIN32)
        return static_cast<long>(GetCurrentProcessId());
#else
        return static_cast<long>(getpid());
#endif
    }

    std::string OperatingSystem()
    {
        const int bits = static_cast<int>(sizeof(void*) * 8);
#if defined(_WIN32)
        return "Windows " + std::to_string(bits) + "-bit";
#elif defined(__linux__)
        return "Linux " + std::to_string(bits) + "-bit";
#else
        return "Desconhecido " + std::to_string(bits) + "-bit";
#endif
    }

    std::string CreateRunDirectory()
    {
        const fs::path base = ExecutableDirectory() / "logs";
        const std::string runName = TimeHelper::FolderTimestamp() + "_" +
                                    MakeSafeFileName(MachineName()) + "_PID" +
                                    std::to_string(ProcessId());

        for (int i = 0; i < 100; ++i)
        {
            const std::string suffix = i == 0 ? "" : "-" + std::to_string(i);
            const fs::path runDir = base / (runName + suffix);

            if (fs::exists(runDir))
            {
                continue;
            }

            std::error_code ec;
            if (fs::create_directories(runDir, ec) && !ec)
            {
                return runDir.string();
            }
        }

        throw std::runtime_error("Nao foi possivel criar uma pasta unica para a execucao.");
    }
}
