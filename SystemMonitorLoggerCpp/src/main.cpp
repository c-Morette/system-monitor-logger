#include <csignal>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "AppSettings.hpp"
#include "models/MonitorSummary.hpp"
#include "platform/MetricsProvider.hpp"
#include "platform/ProcessProvider.hpp"
#include "services/CsvLogService.hpp"
#include "services/InteractiveSettings.hpp"
#include "services/MonitorService.hpp"
#include "services/ReportService.hpp"
#include "services/SmartService.hpp"
#include "utils/Args.hpp"
#include "utils/FileHelper.hpp"

#if defined(_WIN32)
#include "platform/WindowsMetricsProvider.hpp"
#include "platform/WindowsProcessProvider.hpp"
#else
#include "platform/LinuxMetricsProvider.hpp"
#include "platform/LinuxProcessProvider.hpp"
#endif

namespace
{
    std::unique_ptr<MetricsProvider> CreateMetricsProvider()
    {
#if defined(_WIN32)
        return std::make_unique<WindowsMetricsProvider>();
#else
        return std::make_unique<LinuxMetricsProvider>();
#endif
    }

    std::unique_ptr<ProcessProvider> CreateProcessProvider()
    {
#if defined(_WIN32)
        return std::make_unique<WindowsProcessProvider>();
#else
        return std::make_unique<LinuxProcessProvider>();
#endif
    }

    void OnSignal(int)
    {
        MonitorService::RequestStop();
    }
}

int main(int argc, char** argv)
{
    AppSettings settings;
    if (argc == 1)
    {
        // Sem argumentos: abre o assistente interativo.
        if (!InteractiveSettings::Ask(settings))
        {
            std::printf("Monitoramento cancelado.\n");
            return 0;
        }
    }
    else
    {
        std::string error;
        if (!Args::Parse(argc, argv, settings, error))
        {
            if (error == "help")
            {
                std::printf("%s", Args::HelpText().c_str());
                return 0;
            }
            std::fprintf(stderr, "%s\n\n%s", error.c_str(), Args::HelpText().c_str());
            return 1;
        }
    }

    std::signal(SIGINT, OnSignal);

    auto metrics = CreateMetricsProvider();
    auto processes = CreateProcessProvider();
    const std::string runDir = FileHelper::CreateRunDirectory();
    CsvLogService csv(runDir);

    MonitorSummary summary;
    summary.machineName = FileHelper::MachineName();
    summary.operatingSystem = FileHelper::OperatingSystem();
    summary.intervalSeconds = settings.intervalSeconds;
    summary.startedAt = std::time(nullptr);

    std::printf("SystemMonitorLogger (C++)\n");
    std::printf("Pasta da execucao: %s\n", runDir.c_str());

    if (settings.smartEnabled)
    {
        std::printf("Coletando SMART (smartctl do sistema)...\n");
        SmartService smart(runDir);
        summary.smart = smart.Collect();
        std::printf("  SMART: %s\n", summary.smart->status.c_str());
    }

    MonitorService monitor(settings, *metrics, *processes, csv);
    monitor.Run(summary);

    ReportService report;
    const std::string reportPath = report.Generate(summary, runDir);

    std::printf("\nEncerrado. %d amostras coletadas.\n",
                static_cast<int>(summary.samples.size()));
    std::printf("  CSV:       %s\n", (runDir + "/samples.csv").c_str());
    std::printf("  Relatorio: %s\n", reportPath.c_str());
    return 0;
}
