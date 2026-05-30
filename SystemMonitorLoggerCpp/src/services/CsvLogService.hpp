#pragma once

#include <string>
#include <vector>

#include "models/ProcessSample.hpp"
#include "models/SystemSample.hpp"

// Grava samples.csv e processes.csv na pasta da execucao.
class CsvLogService
{
public:
    explicit CsvLogService(const std::string& runDirectory);

    void AppendSystemSample(const SystemSample& sample);
    void AppendProcessSamples(const std::vector<ProcessSample>& samples);

private:
    std::string m_samplesPath;
    std::string m_processesPath;
};
