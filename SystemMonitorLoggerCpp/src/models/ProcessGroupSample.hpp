#pragma once

#include <string>

// Processos com o mesmo nome amigavel agrupados (para aproximar do
// Gerenciador de Tarefas). Usado no relatorio.
struct ProcessGroupSample
{
    std::string timestamp;
    std::string processName;
    std::string displayName;
    int processCount = 0;
    double cpuPercent = 0.0;
    double memoryMb = 0.0;
};
