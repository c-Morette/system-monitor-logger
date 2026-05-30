#pragma once

#include <vector>

#include "models/ProcessSample.hpp"

// Classe base abstrata para coleta de processos por plataforma.
// CollectAll() retorna TODOS os processos com cpu%/memoria preenchidos; a
// selecao dos "top" fica no ProcessService (logica comum).
class ProcessProvider
{
public:
    virtual ~ProcessProvider() = default;

    virtual std::vector<ProcessSample> CollectAll() = 0;
};
