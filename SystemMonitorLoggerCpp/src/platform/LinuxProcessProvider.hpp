#pragma once

#include "platform/ProcessProvider.hpp"

#include <chrono>
#include <cstdint>
#include <unordered_map>

// Coleta de processos no Linux via /proc. Mantem estado do tempo de CPU
// anterior por PID para calcular o uso percentual entre amostras.
class LinuxProcessProvider : public ProcessProvider
{
public:
    std::vector<ProcessSample> CollectAll() override;

private:
    struct CpuSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        std::uint64_t totalTicks = 0;
    };

    std::unordered_map<std::int32_t, CpuSnapshot> m_previous;
};
