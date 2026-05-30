#pragma once

#include "platform/ProcessProvider.hpp"

#include <chrono>
#include <cstdint>
#include <unordered_map>

// Coleta de processos no Windows via Toolhelp32 + GetProcessTimes +
// GetProcessMemoryInfo. Mantem o tempo de CPU anterior por PID.
class WindowsProcessProvider : public ProcessProvider
{
public:
    std::vector<ProcessSample> CollectAll() override;

private:
    struct CpuSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        std::uint64_t totalTicks = 0; // 100ns (kernel+user)
    };

    std::unordered_map<std::int32_t, CpuSnapshot> m_previous;
};
