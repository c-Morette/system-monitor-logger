#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "models/ProcessSample.hpp"
#include "models/SmartResult.hpp"
#include "models/SystemSample.hpp"

// Resumo de uma execucao, base para o relatorio final.
struct MonitorSummary
{
    std::string machineName;
    std::string operatingSystem;
    std::time_t startedAt = 0;
    std::time_t finishedAt = 0;
    int intervalSeconds = 1;
    std::vector<SystemSample> samples;
    std::vector<ProcessSample> processSamples;
    std::optional<SmartResult> smart;
};
