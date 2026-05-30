#pragma once

#include <vector>

#include "models/ProcessSample.hpp"

// Logica comum (independente de plataforma) de selecao dos processos "top".
class ProcessService
{
public:
    // A partir de TODOS os processos, retorna os top por memoria e por CPU,
    // sem duplicar PIDs (mesma regra do projeto C#).
    static std::vector<ProcessSample> SelectTop(const std::vector<ProcessSample>& all, int count);
};
