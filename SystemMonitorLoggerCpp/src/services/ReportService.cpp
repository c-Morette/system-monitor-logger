#include "services/ReportService.hpp"
#include "models/ProcessGroupSample.hpp"
#include "models/SmartResult.hpp"
#include "utils/Format.hpp"
#include "utils/TimeHelper.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    struct Diagnosis
    {
        std::string primaryBottleneck;
        std::string description;
        std::vector<std::string> recommendations;
        std::vector<std::string> evidence;
    };

    // Estatisticas agregadas das amostras de sistema.
    struct Stats
    {
        std::size_t count = 0;
        double cpuAvg = 0, cpuPeak = 0;
        int cpuHigh = 0;
        double memAvg = 0, memPeak = 0;
        int memHigh = 0;
        double diskAvg = 0, diskPeak = 0;
        int diskHigh = 0;
        double readAvg = 0, readPeak = 0;
        double writeAvg = 0, writePeak = 0;
        double ioPeak = 0;
        int ioHigh = 0;
        double lastFreeMb = 0, lastTotalMb = 0;
        double freePercent = 100;
    };

    Stats Compute(const std::vector<SystemSample>& s)
    {
        Stats st;
        st.count = s.size();
        if (s.empty())
        {
            return st;
        }

        double cpuSum = 0, memSum = 0, diskSum = 0, readSum = 0, writeSum = 0;
        for (const auto& x : s)
        {
            cpuSum += x.cpuPercent;
            memSum += x.memoryUsedPercent;
            diskSum += x.diskUsagePercent;
            readSum += x.diskReadMbPerSecond;
            writeSum += x.diskWriteMbPerSecond;

            st.cpuPeak = std::max(st.cpuPeak, x.cpuPercent);
            st.memPeak = std::max(st.memPeak, x.memoryUsedPercent);
            st.diskPeak = std::max(st.diskPeak, x.diskUsagePercent);
            st.readPeak = std::max(st.readPeak, x.diskReadMbPerSecond);
            st.writePeak = std::max(st.writePeak, x.diskWriteMbPerSecond);

            const double io = x.diskReadMbPerSecond + x.diskWriteMbPerSecond;
            st.ioPeak = std::max(st.ioPeak, io);

            if (x.cpuPercent >= 90) ++st.cpuHigh;
            if (x.memoryUsedPercent >= 90) ++st.memHigh;
            if (x.diskUsagePercent >= 95) ++st.diskHigh;
            if (io >= 50) ++st.ioHigh;
        }

        const double n = static_cast<double>(s.size());
        st.cpuAvg = cpuSum / n;
        st.memAvg = memSum / n;
        st.diskAvg = diskSum / n;
        st.readAvg = readSum / n;
        st.writeAvg = writeSum / n;

        st.lastFreeMb = s.back().diskFreeMb;
        st.lastTotalMb = s.back().diskTotalMb;
        st.freePercent = st.lastTotalMb <= 0 ? 100 : st.lastFreeMb * 100.0 / st.lastTotalMb;
        return st;
    }

    double Score(double average, int highSamples, std::size_t total,
                 double averageThreshold, double highThreshold)
    {
        double score = 0;
        if (average >= averageThreshold)
        {
            score += 2;
        }
        if (highSamples >= std::max(1.0, static_cast<double>(total) * 0.2))
        {
            score += highThreshold >= 95 ? 2 : 1.5;
        }
        return score;
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Agrupa processos pelo nome amigavel. Para cada nome, pega o pico (entre
    // os timestamps) da soma de CPU/RAM dos PIDs daquele nome. (Logica do C#.)
    std::vector<ProcessGroupSample> GroupProcesses(const std::vector<ProcessSample>& samples)
    {
        struct TsAgg
        {
            std::string display;
            std::string proc;
            std::set<std::int32_t> pids;
            double cpu = 0;
            double mem = 0;
        };
        // timestamp -> displayLower -> agregado naquele instante
        std::map<std::string, std::map<std::string, TsAgg>> byTimestamp;

        for (const auto& p : samples)
        {
            const std::string key = ToLower(p.displayName);
            TsAgg& a = byTimestamp[p.timestamp][key];
            if (a.display.empty())
            {
                a.display = p.displayName;
                a.proc = p.processName;
            }
            a.pids.insert(p.pid);
            a.cpu += p.cpuPercent;
            a.mem += p.memoryMb;
        }

        std::map<std::string, ProcessGroupSample> byDisplay;
        for (const auto& [timestamp, displays] : byTimestamp)
        {
            for (const auto& [key, agg] : displays)
            {
                ProcessGroupSample& g = byDisplay[key];
                if (g.displayName.empty())
                {
                    g.displayName = agg.display;
                    g.processName = agg.proc;
                }
                g.timestamp = timestamp;
                g.processCount = std::max(g.processCount, static_cast<int>(agg.pids.size()));
                g.cpuPercent = std::max(g.cpuPercent, agg.cpu);
                g.memoryMb = std::max(g.memoryMb, agg.mem);
            }
        }

        std::vector<ProcessGroupSample> result;
        result.reserve(byDisplay.size());
        for (auto& [key, group] : byDisplay)
        {
            result.push_back(group);
        }
        return result;
    }

    std::vector<std::string> BuildSystemEvidence(const Stats& st,
                                                 const std::vector<ProcessGroupSample>& grouped)
    {
        std::vector<std::string> evidence = {
            "CPU media " + FormatNum(st.cpuAvg) + "% e pico " + FormatNum(st.cpuPeak) + "%.",
            "RAM media " + FormatNum(st.memAvg) + "% e pico " + FormatNum(st.memPeak) + "%.",
            "Espaco em disco usado medio " + FormatNum(st.diskAvg) + "% e pico " + FormatNum(st.diskPeak) + "%.",
            "Atividade de disco: leitura media " + FormatNum(st.readAvg) + " MB/s, pico " + FormatNum(st.readPeak) +
                " MB/s; escrita media " + FormatNum(st.writeAvg) + " MB/s, pico " + FormatNum(st.writePeak) + " MB/s.",
            "Espaco livre em disco " + FormatNum(st.freePercent) + "%."};

        if (!grouped.empty())
        {
            const auto topCpu = std::max_element(grouped.begin(), grouped.end(),
                [](const ProcessGroupSample& a, const ProcessGroupSample& b) {
                    return a.cpuPercent < b.cpuPercent;
                });
            evidence.push_back("Grupo de processos com maior CPU: " + topCpu->displayName +
                               " (" + std::to_string(topCpu->processCount) + " PIDs) com " +
                               FormatNum(topCpu->cpuPercent) + "%.");

            const auto topMem = std::max_element(grouped.begin(), grouped.end(),
                [](const ProcessGroupSample& a, const ProcessGroupSample& b) {
                    return a.memoryMb < b.memoryMb;
                });
            evidence.push_back("Grupo de processos com maior uso de RAM: " + topMem->displayName +
                               " (" + std::to_string(topMem->processCount) + " PIDs) com " +
                               FormatNum(topMem->memoryMb) + " MB.");
        }

        return evidence;
    }

    void AppendProcessSummary(std::ostringstream& out,
                              const std::vector<ProcessGroupSample>& grouped)
    {
        if (grouped.empty())
        {
            return;
        }

        std::vector<ProcessGroupSample> byCpu = grouped;
        std::sort(byCpu.begin(), byCpu.end(),
                  [](const ProcessGroupSample& a, const ProcessGroupSample& b) {
                      if (a.cpuPercent != b.cpuPercent) return a.cpuPercent > b.cpuPercent;
                      return a.memoryMb > b.memoryMb;
                  });

        out << "Processos com maior uso observado por CPU:\n";
        for (std::size_t i = 0; i < byCpu.size() && i < 5; ++i)
        {
            out << "- " << byCpu[i].displayName << " (" << byCpu[i].processCount
                << " PIDs) - CPU: " << FormatNum(byCpu[i].cpuPercent)
                << "% - RAM: " << FormatNum(byCpu[i].memoryMb) << " MB\n";
        }

        std::vector<ProcessGroupSample> byMem = grouped;
        std::sort(byMem.begin(), byMem.end(),
                  [](const ProcessGroupSample& a, const ProcessGroupSample& b) {
                      if (a.memoryMb != b.memoryMb) return a.memoryMb > b.memoryMb;
                      return a.cpuPercent > b.cpuPercent;
                  });

        out << "\nProcessos com maior uso observado por RAM:\n";
        for (std::size_t i = 0; i < byMem.size() && i < 5; ++i)
        {
            out << "- " << byMem[i].displayName << " (" << byMem[i].processCount
                << " PIDs) - RAM: " << FormatNum(byMem[i].memoryMb)
                << " MB - CPU: " << FormatNum(byMem[i].cpuPercent) << "%\n";
        }
        out << "\n";
    }

    bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                           [](char a, char b) {
                               return std::tolower(static_cast<unsigned char>(a)) ==
                                      std::tolower(static_cast<unsigned char>(b));
                           }) != haystack.end();
    }

    std::vector<std::string> BuildSmartEvidence(const SmartResult& s)
    {
        std::vector<std::string> evidence = {"Status SMART: " + s.status + "."};
        if (!s.device.empty())
        {
            evidence.push_back("Dispositivo analisado: " + s.device + ".");
        }
        if (s.pendingSectors.has_value())
        {
            evidence.push_back("Setores pendentes: " + std::to_string(*s.pendingSectors) + ".");
        }
        if (s.reallocatedSectors.has_value())
        {
            evidence.push_back("Setores realocados: " + std::to_string(*s.reallocatedSectors) + ".");
        }
        if (s.temperatureCelsius.has_value())
        {
            evidence.push_back("Temperatura: " + std::to_string(*s.temperatureCelsius) + " C.");
        }
        return evidence;
    }

    void AppendSmartSummary(std::ostringstream& out, const std::optional<SmartResult>& smart)
    {
        out << "SMART:\n";
        if (!smart.has_value())
        {
            out << "Status: Nao executado\n";
            return;
        }

        const SmartResult& s = *smart;
        out << "Status: " << s.status << "\n";
        if (!s.device.empty())
        {
            out << "Dispositivo: " << s.device << "\n";
        }
        if (s.temperatureCelsius.has_value())
        {
            out << "Temperatura: " << *s.temperatureCelsius << " C\n";
        }
        if (s.powerOnHours.has_value())
        {
            out << "Horas de uso: " << *s.powerOnHours << "\n";
        }
        if (s.reallocatedSectors.has_value())
        {
            out << "Setores realocados: " << *s.reallocatedSectors << "\n";
        }
        if (s.pendingSectors.has_value())
        {
            out << "Setores pendentes: " << *s.pendingSectors << "\n";
        }
        out << s.details << "\n";
    }

    Diagnosis Inconclusive(std::vector<std::string> evidence)
    {
        if (evidence.empty())
        {
            evidence = {"Nenhum limite critico foi atingido durante o periodo monitorado."};
        }
        return {
            "INCONCLUSIVO",
            "Durante o periodo monitorado, nao foram encontrados picos significativos de CPU, memoria ou disco.",
            {"Repetir o teste durante o momento em que a lentidao ocorrer.",
             "Aumentar a duracao do monitoramento.",
             "Comparar os logs gerados em diferentes horarios."},
            evidence};
    }

    Diagnosis Analyze(const Stats& st, const std::vector<ProcessGroupSample>& grouped,
                      const std::optional<SmartResult>& smart)
    {
        // SMART tem prioridade no diagnostico quando disponivel.
        if (smart.has_value())
        {
            const SmartResult& s = *smart;
            const bool failed = (s.smartPassed == std::optional<bool>(false)) ||
                                (s.available && ContainsIgnoreCase(s.status, "failed"));
            if (failed)
            {
                return {"SMART", "O SMART indicou falha ou risco fisico no armazenamento.",
                        {"Fazer backup dos dados importantes.",
                         "Verificar a saude do disco com uma ferramenta dedicada.",
                         "Considerar substituicao do armazenamento."},
                        BuildSmartEvidence(s)};
            }

            const bool warning = (s.pendingSectors.value_or(0) > 0) ||
                                 (s.reallocatedSectors.value_or(0) > 50) ||
                                 (s.temperatureCelsius.value_or(0) > 55);
            if (warning)
            {
                return {"SMART", "O SMART encontrou sinais de alerta no armazenamento.",
                        {"Fazer backup dos dados importantes.",
                         "Verificar o disco com uma ferramenta dedicada.",
                         "Acompanhar se setores pendentes ou realocados aumentam."},
                        BuildSmartEvidence(s)};
            }
        }

        if (st.count == 0)
        {
            return Inconclusive({});
        }

        double cpuScore = Score(st.cpuAvg, st.cpuHigh, st.count, 80, 90);
        double memScore = Score(st.memAvg, st.memHigh, st.count, 85, 90);
        double diskScore = Score(st.diskAvg, st.diskHigh, st.count, 80, 95);

        if (st.freePercent < 5) diskScore += 3;
        else if (st.freePercent < 15) diskScore += 1.5;

        const double ioAvg = st.readAvg + st.writeAvg;
        if (ioAvg >= 50 || st.ioPeak >= 150 ||
            st.ioHigh >= std::max(1.0, static_cast<double>(st.count) * 0.2))
        {
            diskScore += 1.5;
        }

        const auto evidence = BuildSystemEvidence(st, grouped);

        std::string best = "CPU";
        double bestScore = cpuScore;
        if (memScore > bestScore) { best = "MEMORIA"; bestScore = memScore; }
        if (diskScore > bestScore) { best = "DISCO"; bestScore = diskScore; }

        if (bestScore < 2)
        {
            return Inconclusive(evidence);
        }

        if (best == "CPU")
        {
            return {"CPU", "A CPU apresentou uso alto durante o monitoramento.",
                    {"Verificar os processos com maior consumo.",
                     "Repetir o teste durante o momento de lentidao.",
                     "Avaliar se ha tarefas em segundo plano consumindo processamento."},
                    evidence};
        }
        if (best == "MEMORIA")
        {
            return {"MEMORIA", "A memoria RAM apresentou uso alto ou pouca folga durante o monitoramento.",
                    {"Verificar os processos que mais consomem memoria.",
                     "Fechar programas desnecessarios durante o uso do sistema.",
                     "Considerar expansao de RAM se o uso alto for constante."},
                    evidence};
        }
        return {"DISCO", "O disco apresentou uso elevado ou pouco espaco livre, o que pode causar lentidao.",
                {"Verificar se o computador utiliza HD mecanico.",
                 "Liberar espaco em disco se houver pouco espaco livre.",
                 "Considerar troca para SSD.",
                 "Verificar a saude SMART do disco."},
                evidence};
    }

    void AppendMetric(std::ostringstream& out, const std::string& name,
                      double average, double peak)
    {
        out << name << " media: " << FormatNum(average) << "%\n";
        out << name << " pico: " << FormatNum(peak) << "%\n\n";
    }

    void AppendDiskValidation(std::ostringstream& out, const Stats& st)
    {
        if (st.count == 0)
        {
            return;
        }

        const double ioAvg = st.readAvg + st.writeAvg;
        const double highIoPercent = st.count == 0 ? 0 : st.ioHigh * 100.0 / static_cast<double>(st.count);

        const std::string spaceStatus =
            (st.freePercent < 5 || st.diskPeak >= 98) ? "CRITICO"
            : (st.freePercent < 15 || st.diskPeak >= 90) ? "ATENCAO"
                                                         : "OK";
        const std::string ioStatus =
            (ioAvg >= 50 || st.ioPeak >= 150 || highIoPercent >= 20) ? "ALTO"
            : (ioAvg >= 20 || st.ioPeak >= 80 || highIoPercent >= 10) ? "MODERADO"
                                                                      : "NORMAL";

        out << "Validacao rapida do disco:\n";
        out << "- Espaco ocupado: " << spaceStatus << " - pico " << FormatNum(st.diskPeak)
            << "% usado, " << FormatNum(st.freePercent) << "% livre.\n";
        out << "- Atividade de disco: " << ioStatus << " - media " << FormatNum(ioAvg)
            << " MB/s, pico " << FormatNum(st.ioPeak) << " MB/s.\n";
        out << "- Amostras com I/O alto: " << st.ioHigh << " de " << st.count
            << " (" << FormatNum(highIoPercent) << "%).\n";
        out << "Observacao: espaco ocupado e atividade de disco sao coisas diferentes; "
               "o CSV guarda ambos separadamente.\n\n";
    }

    std::string BuildReport(const MonitorSummary& summary)
    {
        const Stats st = Compute(summary.samples);
        const std::vector<ProcessGroupSample> grouped = GroupProcesses(summary.processSamples);
        const Diagnosis diag = Analyze(st, grouped, summary.smart);

        std::ostringstream out;
        out << "SystemMonitorLogger - Relatorio de Diagnostico\n\n";
        out << "Computador: " << summary.machineName << "\n";
        out << "Sistema: " << summary.operatingSystem << "\n";
        out << "Inicio: " << TimeHelper::FormatDateTime(summary.startedAt) << "\n";
        out << "Fim: " << TimeHelper::FormatDateTime(summary.finishedAt) << "\n";
        out << "Duracao: " << TimeHelper::FormatDuration(
                   static_cast<long long>(summary.finishedAt - summary.startedAt)) << "\n";
        out << "Intervalo: " << summary.intervalSeconds << " segundos\n\n";

        if (st.count == 0)
        {
            out << "Nenhuma amostra foi coletada.\n";
            return out.str();
        }

        out << "Resumo:\n";
        AppendMetric(out, "CPU", st.cpuAvg, st.cpuPeak);
        AppendMetric(out, "RAM", st.memAvg, st.memPeak);
        AppendMetric(out, "Espaco em disco", st.diskAvg, st.diskPeak);
        out << "Leitura de disco media: " << FormatNum(st.readAvg) << " MB/s\n";
        out << "Leitura de disco pico: " << FormatNum(st.readPeak) << " MB/s\n";
        out << "Escrita de disco media: " << FormatNum(st.writeAvg) << " MB/s\n";
        out << "Escrita de disco pico: " << FormatNum(st.writePeak) << " MB/s\n\n";
        out << "Espaco livre em disco: " << FormatNum(st.lastFreeMb / 1024.0) << " GB de "
            << FormatNum(st.lastTotalMb / 1024.0) << " GB\n\n";

        AppendDiskValidation(out, st);
        AppendProcessSummary(out, grouped);
        AppendSmartSummary(out, summary.smart);

        out << "\nDiagnostico provavel:\n";
        out << "Possivel gargalo principal: " << diag.primaryBottleneck << "\n";
        out << diag.description << "\n\n";
        out << "Evidencias:\n";
        for (const auto& e : diag.evidence)
        {
            out << "- " << e << "\n";
        }

        out << "\nRecomendacoes:\n";
        int item = 1;
        for (const auto& r : diag.recommendations)
        {
            out << item++ << ". " << r << "\n";
        }

        return out.str();
    }
}

std::string ReportService::Generate(const MonitorSummary& summary, const std::string& runDirectory)
{
    const std::string reportPath = (fs::path(runDirectory) / "report.txt").string();
    std::ofstream file(reportPath, std::ios::trunc | std::ios::binary);
    file << BuildReport(summary);
    return reportPath;
}
