#include "services/CsvLogService.hpp"
#include "utils/Format.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
    const char* kSamplesHeader =
        "timestamp,cpu_percent,memory_used_percent,memory_used_mb,memory_total_mb,"
        "disk_usage_percent,disk_free_mb,disk_total_mb,disk_read_mb_s,disk_write_mb_s\n";

    const char* kProcessesHeader =
        "timestamp,process_name,display_name,pid,cpu_percent,memory_mb\n";

    // Escapa campos de texto para CSV (aspas se contiver virgula/aspas/quebra).
    std::string Escape(const std::string& value)
    {
        if (value.find_first_of(",\"\n\r") == std::string::npos)
        {
            return value;
        }
        std::string escaped = "\"";
        for (char c : value)
        {
            if (c == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped += c;
            }
        }
        escaped += '"';
        return escaped;
    }
}

CsvLogService::CsvLogService(const std::string& runDirectory)
{
    m_samplesPath = (fs::path(runDirectory) / "samples.csv").string();
    m_processesPath = (fs::path(runDirectory) / "processes.csv").string();

    std::ofstream samples(m_samplesPath, std::ios::trunc | std::ios::binary);
    samples << kSamplesHeader;

    std::ofstream processes(m_processesPath, std::ios::trunc | std::ios::binary);
    processes << kProcessesHeader;
}

void CsvLogService::AppendSystemSample(const SystemSample& sample)
{
    std::ofstream file(m_samplesPath, std::ios::app | std::ios::binary);
    if (!file.is_open())
    {
        return;
    }

    file << sample.timestamp << ','
         << FormatNum(sample.cpuPercent) << ','
         << FormatNum(sample.memoryUsedPercent) << ','
         << FormatNum(sample.memoryUsedMb) << ','
         << FormatNum(sample.memoryTotalMb) << ','
         << FormatNum(sample.diskUsagePercent) << ','
         << FormatNum(sample.diskFreeMb) << ','
         << FormatNum(sample.diskTotalMb) << ','
         << FormatNum(sample.diskReadMbPerSecond) << ','
         << FormatNum(sample.diskWriteMbPerSecond) << '\n';
}

void CsvLogService::AppendProcessSamples(const std::vector<ProcessSample>& samples)
{
    if (samples.empty())
    {
        return;
    }

    std::ofstream file(m_processesPath, std::ios::app | std::ios::binary);
    if (!file.is_open())
    {
        return;
    }

    for (const auto& s : samples)
    {
        file << s.timestamp << ','
             << Escape(s.processName) << ','
             << Escape(s.displayName) << ','
             << s.pid << ','
             << FormatNum(s.cpuPercent) << ','
             << FormatNum(s.memoryMb) << '\n';
    }
}
