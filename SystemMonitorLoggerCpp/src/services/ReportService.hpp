#pragma once

#include <string>

#include "models/MonitorSummary.hpp"

// Gera report.txt com diagnostico baseado nas amostras de sistema.
// Secoes de processos (Fase 3) e SMART (Fase 4) serao acrescentadas depois.
class ReportService
{
public:
    // Grava o relatorio e retorna o caminho do arquivo.
    std::string Generate(const MonitorSummary& summary, const std::string& runDirectory);
};
