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
    // Percentual de tempo em que o disco fisico ficou ocupado (% Disk Time no
    // Windows / %util no Linux). E' o indicador classico de "disco saturado":
    // perto de 100% sustentado significa que o disco e' o gargalo, mesmo com
    // poucos MB/s (tipico de HD mecanico antigo).
    double diskActivePercent = 0.0;
};
