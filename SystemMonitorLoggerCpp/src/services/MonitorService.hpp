#pragma once

#include "AppSettings.hpp"
#include "models/MonitorSummary.hpp"
#include "platform/MetricsProvider.hpp"
#include "platform/ProcessProvider.hpp"
#include "services/CsvLogService.hpp"

// Loop principal: coleta amostras no intervalo configurado, grava CSV, atualiza
// a tela e preenche o resumo. Para quando a duracao acaba ou ao pedir parada.
class MonitorService
{
public:
    MonitorService(const AppSettings& settings,
                   MetricsProvider& metrics,
                   ProcessProvider& processes,
                   CsvLogService& csv);

    void Run(MonitorSummary& summary);

    // Sinaliza parada (chamado pelo handler de CTRL+C).
    static void RequestStop();
    static bool StopRequested();

private:
    const AppSettings& m_settings;
    MetricsProvider& m_metrics;
    ProcessProvider& m_processes;
    CsvLogService& m_csv;
};
