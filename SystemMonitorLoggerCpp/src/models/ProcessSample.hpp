#pragma once

#include <cstdint>
#include <string>

// Uma amostra de um processo. Campos identicos ao processes.csv do C#.
struct ProcessSample
{
    std::string timestamp;
    std::string processName;
    std::string displayName;
    std::int32_t pid = 0;
    double cpuPercent = 0.0;
    double memoryMb = 0.0;
};
