#pragma once

#include <string>

// Uma amostra do sistema num instante. Campos identicos ao samples.csv do
// projeto C# para manter compatibilidade dos relatorios.
struct SystemSample
{
    std::string timestamp;
    double cpuPercent = 0.0;
    double memoryUsedPercent = 0.0;
    double memoryUsedMb = 0.0;
    double memoryTotalMb = 0.0;
    double diskUsagePercent = 0.0;
    double diskFreeMb = 0.0;
    double diskTotalMb = 0.0;
    double diskReadMbPerSecond = 0.0;
    double diskWriteMbPerSecond = 0.0;
};
