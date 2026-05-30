#include "services/MonitorService.hpp"
#include "services/ProcessService.hpp"
#include "utils/Console.hpp"
#include "utils/Format.hpp"
#include "utils/TimeHelper.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace
{
    std::atomic<bool> g_stop{false};

    // Top processos por CPU (para a tela ao vivo), a partir da ultima coleta.
    std::vector<ProcessSample> TopByCpu(std::vector<ProcessSample> procs, std::size_t n)
    {
        std::sort(procs.begin(), procs.end(),
                  [](const ProcessSample& a, const ProcessSample& b) {
                      if (a.cpuPercent != b.cpuPercent) return a.cpuPercent > b.cpuPercent;
                      return a.memoryMb > b.memoryMb;
                  });
        if (procs.size() > n)
        {
            procs.resize(n);
        }
        return procs;
    }

    // Top processos por RAM (para a tela ao vivo).
    std::vector<ProcessSample> TopByMemory(std::vector<ProcessSample> procs, std::size_t n)
    {
        std::sort(procs.begin(), procs.end(),
                  [](const ProcessSample& a, const ProcessSample& b) {
                      if (a.memoryMb != b.memoryMb) return a.memoryMb > b.memoryMb;
                      return a.cpuPercent > b.cpuPercent;
                  });
        if (procs.size() > n)
        {
            procs.resize(n);
        }
        return procs;
    }
}

MonitorService::MonitorService(const AppSettings& settings,
                               MetricsProvider& metrics,
                               ProcessProvider& processes,
                               CsvLogService& csv)
    : m_settings(settings), m_metrics(metrics), m_processes(processes), m_csv(csv)
{
}

void MonitorService::RequestStop()
{
    g_stop = true;
}

bool MonitorService::StopRequested()
{
    return g_stop;
}

void MonitorService::Run(MonitorSummary& summary)
{
    const std::time_t start = std::time(nullptr);
    const long long intervalMs = static_cast<long long>(m_settings.intervalSeconds) * 1000;
    std::vector<ProcessSample> lastTop;

    if (!m_settings.simpleMode)
    {
        Console::Init();
    }

    while (!g_stop)
    {
        const std::time_t now = std::time(nullptr);
        const long long elapsed = static_cast<long long>(now - start);
        if (m_settings.durationSeconds >= 0 && elapsed >= m_settings.durationSeconds)
        {
            break;
        }

        const SystemSample sample = m_metrics.GetSystemSample();
        m_csv.AppendSystemSample(sample);
        summary.samples.push_back(sample);

        // Processos no mesmo ritmo das metricas (intervalo configurado), para a
        // lista de processos ficar alinhada ao segundo do pico analisado.
        const std::vector<ProcessSample> all = m_processes.CollectAll();
        const std::vector<ProcessSample> top = ProcessService::SelectTop(all, 5);
        m_csv.AppendProcessSamples(top);
        for (const auto& p : top)
        {
            summary.processSamples.push_back(p);
        }
        lastTop = top;

        // --- Tela ---
        if (m_settings.simpleMode)
        {
            std::printf("[%s] cpu %5.1f%%  ram %5.1f%% (%.0f/%.0f MB)  disco %5.1f%%  rd %.2f wr %.2f MB/s\n",
                        sample.timestamp.c_str(), sample.cpuPercent,
                        sample.memoryUsedPercent, sample.memoryUsedMb, sample.memoryTotalMb,
                        sample.diskUsagePercent,
                        sample.diskReadMbPerSecond, sample.diskWriteMbPerSecond);
        }
        else
        {
            const std::string limit = m_settings.durationSeconds < 0
                ? std::string("sem limite")
                : TimeHelper::FormatDuration(m_settings.durationSeconds);

            std::vector<std::string> lines;
            char buf[160];

            lines.push_back("SystemMonitorLogger - monitorando (" + summary.machineName + ")");
            lines.push_back("Tempo: " + TimeHelper::FormatDuration(elapsed) + " / " + limit +
                            "    CTRL+C para encerrar");
            lines.push_back("");

            std::snprintf(buf, sizeof(buf), "  (%.0f/%.0f MB)",
                          sample.memoryUsedMb, sample.memoryTotalMb);
            lines.push_back("CPU   " + Console::Bar(sample.cpuPercent));
            lines.push_back("RAM   " + Console::Bar(sample.memoryUsedPercent) + buf);
            std::snprintf(buf, sizeof(buf), "  (%.1f GB livres)", sample.diskFreeMb / 1024.0);
            lines.push_back("Disco " + Console::Bar(sample.diskUsagePercent) + buf);
            std::snprintf(buf, sizeof(buf), "I/O   leitura %.2f MB/s   escrita %.2f MB/s",
                          sample.diskReadMbPerSecond, sample.diskWriteMbPerSecond);
            lines.push_back(buf);
            lines.push_back("");
            lines.push_back("Top processos por CPU:");
            for (const auto& p : TopByCpu(lastTop, 5))
            {
                std::snprintf(buf, sizeof(buf), "  %-28.28s  CPU %5.1f%%  RAM %8.1f MB",
                              p.displayName.c_str(), p.cpuPercent, p.memoryMb);
                lines.push_back(buf);
            }

            lines.push_back("");
            lines.push_back("Top processos por RAM:");
            for (const auto& p : TopByMemory(lastTop, 5))
            {
                std::snprintf(buf, sizeof(buf), "  %-28.28s  RAM %8.1f MB  CPU %5.1f%%",
                              p.displayName.c_str(), p.memoryMb, p.cpuPercent);
                lines.push_back(buf);
            }

            Console::DrawFrame(lines);
        }

        // Dorme em fatias curtas para responder ao CTRL+C rapidamente.
        for (long long slept = 0; slept < intervalMs && !g_stop; slept += 100)
        {
            TimeHelper::SleepMs(100);
        }
    }

    summary.finishedAt = std::time(nullptr);
}
