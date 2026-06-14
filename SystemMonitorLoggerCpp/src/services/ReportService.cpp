#include "services/ReportService.hpp"
#include "models/ProcessGroupSample.hpp"
#include "models/SmartResult.hpp"
#include "utils/Format.hpp"
#include "utils/TimeHelper.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
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
    // Niveis de severidade do diagnostico. Sempre concluimos um deles.
    enum class Level
    {
        Healthy = 0, // Saudavel
        Warning = 1, // Atencao
        Bottleneck = 2 // Gargalo
    };

    const char* LevelLabel(Level level)
    {
        switch (level)
        {
        case Level::Healthy: return "SAUDAVEL";
        case Level::Warning: return "ATENCAO";
        default: return "GARGALO PROVAVEL";
        }
    }

    struct Resource
    {
        std::string name;
        std::string detail;
        Level level = Level::Healthy;
        double pressure = 0.0;
    };

    // Um episodio em que um recurso ficou acima do limite por tempo relevante.
    struct Incident
    {
        std::string resource; // "CPU" | "MEMORIA" | "ATIVIDADE DE DISCO"
        std::string startTs;
        std::string endTs;
        long long durationSeconds = 0;
        double peak = 0.0;
        std::string culprit;
    };

    struct Verdict
    {
        Level level = Level::Healthy;
        std::string primary;
        std::string headline;
        std::vector<std::string> recommendations;
        std::vector<std::string> evidence;
    };

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
        double activeAvg = 0, activePeak = 0;
        int activeHigh = 0;
        bool hasActive = false;
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

        double cpuSum = 0, memSum = 0, diskSum = 0, readSum = 0, writeSum = 0, activeSum = 0;
        for (const auto& x : s)
        {
            cpuSum += x.cpuPercent;
            memSum += x.memoryUsedPercent;
            diskSum += x.diskUsagePercent;
            readSum += x.diskReadMbPerSecond;
            writeSum += x.diskWriteMbPerSecond;
            activeSum += x.diskActivePercent;

            st.cpuPeak = std::max(st.cpuPeak, x.cpuPercent);
            st.memPeak = std::max(st.memPeak, x.memoryUsedPercent);
            st.diskPeak = std::max(st.diskPeak, x.diskUsagePercent);
            st.readPeak = std::max(st.readPeak, x.diskReadMbPerSecond);
            st.writePeak = std::max(st.writePeak, x.diskWriteMbPerSecond);
            st.activePeak = std::max(st.activePeak, x.diskActivePercent);

            const double io = x.diskReadMbPerSecond + x.diskWriteMbPerSecond;
            st.ioPeak = std::max(st.ioPeak, io);

            if (x.cpuPercent >= 90) ++st.cpuHigh;
            if (x.memoryUsedPercent >= 90) ++st.memHigh;
            if (x.diskUsagePercent >= 95) ++st.diskHigh;
            if (io >= 50) ++st.ioHigh;
            if (x.diskActivePercent >= 85) ++st.activeHigh;
            if (x.diskActivePercent > 0.01) st.hasActive = true;
        }

        const double n = static_cast<double>(s.size());
        st.cpuAvg = cpuSum / n;
        st.memAvg = memSum / n;
        st.diskAvg = diskSum / n;
        st.readAvg = readSum / n;
        st.writeAvg = writeSum / n;
        st.activeAvg = activeSum / n;

        st.lastFreeMb = s.back().diskFreeMb;
        st.lastTotalMb = s.back().diskTotalMb;
        st.freePercent = st.lastTotalMb <= 0 ? 100 : st.lastFreeMb * 100.0 / st.lastTotalMb;
        return st;
    }

    double Pct(int part, std::size_t total)
    {
        return total == 0 ? 0.0 : static_cast<double>(part) * 100.0 / static_cast<double>(total);
    }

    // "YYYY-MM-DD HH:MM:SS" -> time_t (hora local). 0 se nao parsear.
    std::time_t ParseTs(const std::string& ts)
    {
        int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
        if (std::sscanf(ts.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6)
        {
            return 0;
        }
        std::tm tm{};
        tm.tm_year = y - 1900;
        tm.tm_mon = mo - 1;
        tm.tm_mday = d;
        tm.tm_hour = h;
        tm.tm_min = mi;
        tm.tm_sec = s;
        tm.tm_isdst = -1;
        return std::mktime(&tm);
    }

    // "YYYY-MM-DD HH:MM:SS" -> "HH:MM"
    std::string ShortTime(const std::string& ts)
    {
        return ts.size() >= 16 ? ts.substr(11, 5) : ts;
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                           [](char a, char b) {
                               return std::tolower(static_cast<unsigned char>(a)) ==
                                      std::tolower(static_cast<unsigned char>(b));
                           }) != haystack.end();
    }

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

    const ProcessGroupSample* TopBy(const std::vector<ProcessGroupSample>& g, bool byCpu)
    {
        if (g.empty()) return nullptr;
        return &*std::max_element(g.begin(), g.end(),
            [&](const ProcessGroupSample& a, const ProcessGroupSample& b) {
                return byCpu ? a.cpuPercent < b.cpuPercent : a.memoryMb < b.memoryMb;
            });
    }

    // Processo mais provavel culpado num intervalo [startTs, endTs] (inclusive).
    std::string CulpritIn(const std::vector<ProcessSample>& procs,
                          const std::string& startTs, const std::string& endTs, bool byCpu)
    {
        std::vector<ProcessSample> sub;
        for (const auto& p : procs)
        {
            if (p.timestamp >= startTs && p.timestamp <= endTs)
            {
                sub.push_back(p);
            }
        }
        const auto grouped = GroupProcesses(sub);
        const ProcessGroupSample* top = TopBy(grouped, byCpu);
        if (top == nullptr)
        {
            return "indeterminado";
        }
        if (byCpu)
        {
            return top->displayName + " (CPU " + FormatNum(top->cpuPercent) + "%)";
        }
        return top->displayName + " (RAM " + FormatNum(top->memoryMb) + " MB)";
    }

    enum class Metric { Cpu, Ram, DiskActive };

    double MetricValue(const SystemSample& s, Metric m)
    {
        switch (m)
        {
        case Metric::Cpu: return s.cpuPercent;
        case Metric::Ram: return s.memoryUsedPercent;
        default: return s.diskActivePercent;
        }
    }

    // Encontra episodios sustentados (>= minSeconds) de um recurso.
    void DetectInto(std::vector<Incident>& out, const std::vector<SystemSample>& s,
                    const std::vector<ProcessSample>& procs, Metric m, double threshold,
                    const char* label, bool culpritByCpu, int intervalSeconds, long long minSeconds)
    {
        bool inEpisode = false;
        Incident cur;
        auto close = [&]() {
            const long long span = std::max<long long>(0, ParseTs(cur.endTs) - ParseTs(cur.startTs));
            cur.durationSeconds = span + std::max(1, intervalSeconds);
            if (cur.durationSeconds >= minSeconds)
            {
                cur.culprit = CulpritIn(procs, cur.startTs, cur.endTs, culpritByCpu);
                out.push_back(cur);
            }
        };

        for (const auto& x : s)
        {
            const double v = MetricValue(x, m);
            if (v >= threshold)
            {
                if (!inEpisode)
                {
                    inEpisode = true;
                    cur = Incident{};
                    cur.resource = label;
                    cur.startTs = x.timestamp;
                    cur.peak = v;
                }
                cur.endTs = x.timestamp;
                cur.peak = std::max(cur.peak, v);
            }
            else if (inEpisode)
            {
                close();
                inEpisode = false;
            }
        }
        if (inEpisode)
        {
            close();
        }
    }

    std::vector<Incident> DetectIncidents(const std::vector<SystemSample>& s,
                                          const std::vector<ProcessSample>& procs,
                                          int intervalSeconds, const IncidentConfig& cfg)
    {
        std::vector<Incident> out;
        DetectInto(out, s, procs, Metric::Cpu, cfg.cpu, "CPU", true, intervalSeconds, cfg.minSeconds);
        DetectInto(out, s, procs, Metric::Ram, cfg.ram, "MEMORIA", false, intervalSeconds, cfg.minSeconds);
        DetectInto(out, s, procs, Metric::DiskActive, cfg.diskActive, "ATIVIDADE DE DISCO", true,
                   intervalSeconds, cfg.minSeconds);
        std::sort(out.begin(), out.end(),
                  [](const Incident& a, const Incident& b) { return a.startTs < b.startTs; });
        return out;
    }

    void AppendTimeline(std::ostringstream& out, const std::vector<Incident>& incidents,
                        long long minSeconds)
    {
        out << "Linha do tempo de incidentes (momentos que merecem atencao):\n";
        if (incidents.empty())
        {
            out << "- Nenhum incidente sustentado (>= " << minSeconds
                << "s acima do limite) foi detectado no periodo.\n\n";
            return;
        }
        for (const auto& inc : incidents)
        {
            out << "- [" << ShortTime(inc.startTs) << " -> " << ShortTime(inc.endTs) << "] ("
                << TimeHelper::FormatDuration(inc.durationSeconds) << ")  " << inc.resource
                << "  pico " << FormatNum(inc.peak) << "%  | provavel culpado: " << inc.culprit << "\n";
        }
        out << "\n";
    }

    void AppendHourly(std::ostringstream& out, const std::vector<SystemSample>& s)
    {
        if (s.size() < 2)
        {
            return;
        }

        struct Bucket
        {
            std::size_t n = 0;
            double cpuSum = 0, cpuPeak = 0;
            double memSum = 0, memPeak = 0;
            double actSum = 0, actPeak = 0;
        };
        std::map<std::string, Bucket> buckets; // chave "YYYY-MM-DD HH"
        bool hasActive = false;
        for (const auto& x : s)
        {
            const std::string key = x.timestamp.size() >= 13 ? x.timestamp.substr(0, 13) : x.timestamp;
            Bucket& b = buckets[key];
            ++b.n;
            b.cpuSum += x.cpuPercent;
            b.cpuPeak = std::max(b.cpuPeak, x.cpuPercent);
            b.memSum += x.memoryUsedPercent;
            b.memPeak = std::max(b.memPeak, x.memoryUsedPercent);
            b.actSum += x.diskActivePercent;
            b.actPeak = std::max(b.actPeak, x.diskActivePercent);
            if (x.diskActivePercent > 0.01) hasActive = true;
        }

        if (buckets.size() < 2)
        {
            return; // menos de uma hora: a tabela horaria nao agrega nada util
        }

        out << "Resumo por hora (media | pico):\n";
        for (const auto& [key, b] : buckets)
        {
            const double n = static_cast<double>(b.n);
            char line[200];
            const std::string hour = key.size() >= 13 ? key.substr(11, 2) : key;
            if (hasActive)
            {
                std::snprintf(line, sizeof(line),
                    "  %sh  CPU %3.0f|%3.0f%%   RAM %3.0f|%3.0f%%   Disco %3.0f|%3.0f%%",
                    hour.c_str(), b.cpuSum / n, b.cpuPeak, b.memSum / n, b.memPeak,
                    b.actSum / n, b.actPeak);
            }
            else
            {
                std::snprintf(line, sizeof(line),
                    "  %sh  CPU %3.0f|%3.0f%%   RAM %3.0f|%3.0f%%",
                    hour.c_str(), b.cpuSum / n, b.cpuPeak, b.memSum / n, b.memPeak);
            }
            out << line << "\n";
        }
        out << "\n";
    }

    // Detecta RAM subindo de forma sustentada (sinal de vazamento de memoria).
    void AppendTrend(std::ostringstream& out, const std::vector<SystemSample>& s,
                     const std::vector<ProcessSample>& procs)
    {
        if (s.size() < 6)
        {
            return;
        }

        const std::size_t window = std::max<std::size_t>(1, s.size() / 5); // 20%
        double firstSum = 0, lastSum = 0;
        for (std::size_t i = 0; i < window; ++i)
        {
            firstSum += s[i].memoryUsedPercent;
            lastSum += s[s.size() - 1 - i].memoryUsedPercent;
        }
        const double firstAvg = firstSum / static_cast<double>(window);
        const double lastAvg = lastSum / static_cast<double>(window);
        const double growth = lastAvg - firstAvg;

        out << "Tendencia de memoria RAM:\n";
        if (growth < 8.0)
        {
            out << "- Estavel: variou de " << FormatNum(firstAvg) << "% para " << FormatNum(lastAvg)
                << "% (sem sinal de vazamento).\n\n";
            return;
        }

        out << "- ALERTA de possivel vazamento: RAM subiu de " << FormatNum(firstAvg) << "% para "
            << FormatNum(lastAvg) << "% ao longo do periodo (+" << FormatNum(growth)
            << " pontos), sem voltar.\n";

        // Qual processo cresceu mais em memoria entre o inicio e o fim.
        const std::string firstStart = s.front().timestamp;
        const std::string firstEnd = s[window - 1].timestamp;
        const std::string lastStart = s[s.size() - window].timestamp;
        const std::string lastEnd = s.back().timestamp;

        std::vector<ProcessSample> earlySub, lateSub;
        for (const auto& p : procs)
        {
            if (p.timestamp >= firstStart && p.timestamp <= firstEnd) earlySub.push_back(p);
            if (p.timestamp >= lastStart && p.timestamp <= lastEnd) lateSub.push_back(p);
        }
        std::map<std::string, double> earlyMem, lateMem;
        for (const auto& g : GroupProcesses(earlySub)) earlyMem[g.displayName] = g.memoryMb;
        std::string worstName;
        double worstGrowth = 0;
        for (const auto& g : GroupProcesses(lateSub))
        {
            const auto it = earlyMem.find(g.displayName);
            const double before = it == earlyMem.end() ? 0.0 : it->second;
            const double delta = g.memoryMb - before;
            if (delta > worstGrowth)
            {
                worstGrowth = delta;
                worstName = g.displayName;
            }
        }
        if (!worstName.empty() && worstGrowth > 50.0)
        {
            out << "- Processo que mais cresceu em RAM: " << worstName << " (+"
                << FormatNum(worstGrowth) << " MB). Forte candidato ao vazamento.\n";
        }
        out << "\n";
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

        if (st.hasActive)
        {
            evidence.push_back("Tempo de disco ocupado: media " + FormatNum(st.activeAvg) +
                               "%, pico " + FormatNum(st.activePeak) + "%.");
        }

        if (const ProcessGroupSample* topCpu = TopBy(grouped, true))
        {
            evidence.push_back("Grupo de processos com maior CPU: " + topCpu->displayName +
                               " (" + std::to_string(topCpu->processCount) + " PIDs) com " +
                               FormatNum(topCpu->cpuPercent) + "%.");
        }
        if (const ProcessGroupSample* topMem = TopBy(grouped, false))
        {
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

        out << "Top consumidores do periodo (pico observado) por CPU:\n";
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

        out << "\nTop consumidores do periodo (pico observado) por RAM:\n";
        for (std::size_t i = 0; i < byMem.size() && i < 5; ++i)
        {
            out << "- " << byMem[i].displayName << " (" << byMem[i].processCount
                << " PIDs) - RAM: " << FormatNum(byMem[i].memoryMb)
                << " MB - CPU: " << FormatNum(byMem[i].cpuPercent) << "%\n";
        }
        out << "\n";
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
        out << "SMART (saude fisica do disco):\n";
        if (!smart.has_value())
        {
            out << "Status: Nao executado (rode com SMART habilitado e como admin/root para incluir).\n";
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
        out << "(Detalhes completos do SMART em smart.txt)\n";
    }

    // --- Avaliacao por recurso (graduada, usada quando NAO ha incidentes) ----

    Resource EvalCpu(const Stats& st)
    {
        const double highPct = Pct(st.cpuHigh, st.count);
        Resource r;
        r.name = "CPU";
        r.detail = "uso medio " + FormatNum(st.cpuAvg) + "% e pico " + FormatNum(st.cpuPeak) + "%";
        if (st.cpuAvg >= 85 || highPct >= 40)
            r.level = Level::Bottleneck;
        else if (st.cpuAvg >= 65 || highPct >= 15)
            r.level = Level::Warning;
        r.pressure = st.cpuAvg + highPct * 0.5;
        return r;
    }

    Resource EvalRam(const Stats& st)
    {
        const double highPct = Pct(st.memHigh, st.count);
        Resource r;
        r.name = "MEMORIA";
        r.detail = "uso medio " + FormatNum(st.memAvg) + "% e pico " + FormatNum(st.memPeak) + "%";
        if (st.memAvg >= 90 || highPct >= 40)
            r.level = Level::Bottleneck;
        else if (st.memAvg >= 80 || highPct >= 15)
            r.level = Level::Warning;
        r.pressure = st.memAvg;
        return r;
    }

    Resource EvalDiskSpace(const Stats& st)
    {
        Resource r;
        r.name = "ESPACO EM DISCO";
        r.detail = FormatNum(st.freePercent) + "% livres (" + FormatNum(st.lastFreeMb / 1024.0) + " GB)";
        if (st.freePercent < 5)
            r.level = Level::Bottleneck;
        else if (st.freePercent < 12)
            r.level = Level::Warning;
        r.pressure = st.freePercent < 20 ? (20.0 - st.freePercent) * 6.0 : 0.0;
        return r;
    }

    Resource EvalDiskActivity(const Stats& st)
    {
        const double ioAvg = st.readAvg + st.writeAvg;
        Resource r;
        r.name = "ATIVIDADE DE DISCO";

        if (st.hasActive)
        {
            const double highPct = Pct(st.activeHigh, st.count);
            r.detail = FormatNum(st.activeAvg) + "% de tempo ocupado em media (pico " +
                       FormatNum(st.activePeak) + "%), " + FormatNum(ioAvg) + " MB/s medios";
            if (st.activeAvg >= 80 || highPct >= 40)
                r.level = Level::Bottleneck;
            else if (st.activeAvg >= 50 || highPct >= 15)
                r.level = Level::Warning;
            r.pressure = st.activeAvg;
        }
        else
        {
            const double ioHighPct = Pct(st.ioHigh, st.count);
            r.detail = FormatNum(ioAvg) + " MB/s medios, pico " + FormatNum(st.ioPeak) + " MB/s";
            if (ioAvg >= 80 || st.ioPeak >= 250 || ioHighPct >= 30)
                r.level = Level::Bottleneck;
            else if (ioAvg >= 30 || st.ioPeak >= 120 || ioHighPct >= 15)
                r.level = Level::Warning;
            r.pressure = std::min(ioAvg, 100.0);
        }
        return r;
    }

    std::vector<std::string> RecommendFor(const std::string& primary)
    {
        if (primary == "CPU")
            return {"Identifique no topo da lista os processos que mais consomem CPU.",
                    "Feche ou atualize o programa que dispara o uso alto.",
                    "Repita o monitoramento durante o momento de lentidao para confirmar."};
        if (primary == "MEMORIA")
            return {"Veja os processos que mais consomem RAM e feche os desnecessarios.",
                    "Reinicie programas que crescem com o tempo (vazamento de memoria).",
                    "Se o uso alto for constante, considere ampliar a memoria RAM."};
        if (primary == "ESPACO EM DISCO")
            return {"Libere espaco: limpeza de disco, arquivos temporarios e Lixeira.",
                    "Mova arquivos grandes para outro disco ou nuvem.",
                    "Mantenha pelo menos 15% livres para o sistema respirar."};
        if (primary == "ATIVIDADE DE DISCO")
            return {"Verifique se o disco e' HD mecanico - se for, e o principal suspeito da lentidao.",
                    "Identifique o processo que mais le/grava em disco.",
                    "Considere migrar o sistema para um SSD.",
                    "Confira a saude SMART do disco."};
        return {"Repita o teste durante o momento da lentidao.",
                "Aumente a duracao do monitoramento.",
                "Compare os logs gerados em horarios diferentes."};
    }

    Verdict Analyze(const Stats& st, const std::vector<ProcessGroupSample>& grouped,
                    const std::optional<SmartResult>& smart, const std::vector<Incident>& incidents,
                    long long durationSeconds)
    {
        // 1) SMART tem prioridade absoluta quando aponta falha/alerta fisico.
        if (smart.has_value())
        {
            const SmartResult& s = *smart;
            const bool failed = (s.smartPassed == std::optional<bool>(false)) ||
                                (s.available && ContainsIgnoreCase(s.status, "failed"));
            if (failed)
            {
                return {Level::Bottleneck, "SMART",
                        "O SMART indicou FALHA ou risco fisico no armazenamento. Trate como urgente.",
                        {"Faca backup dos dados importantes agora.",
                         "Verifique o disco com uma ferramenta dedicada.",
                         "Considere substituir o armazenamento."},
                        BuildSmartEvidence(s)};
            }
            const bool warning = (s.pendingSectors.value_or(0) > 0) ||
                                 (s.reallocatedSectors.value_or(0) > 50) ||
                                 (s.temperatureCelsius.value_or(0) > 55);
            if (warning)
            {
                return {Level::Warning, "SMART",
                        "O SMART encontrou sinais de alerta no armazenamento.",
                        {"Faca backup dos dados importantes.",
                         "Verifique o disco com uma ferramenta dedicada.",
                         "Acompanhe se setores pendentes ou realocados aumentam."},
                        BuildSmartEvidence(s)};
            }
        }

        std::vector<std::string> evidence =
            st.count == 0 ? std::vector<std::string>{"Nenhuma amostra coletada."}
                          : BuildSystemEvidence(st, grouped);

        if (st.count == 0)
        {
            return {Level::Healthy, "NENHUM",
                    "Nenhuma amostra foi coletada - o monitoramento foi curto demais.",
                    {"Rode novamente por pelo menos alguns segundos."}, evidence};
        }

        // 2) Se houve incidentes sustentados, eles MANDAM no veredito (essencial
        // para testes longos: um pico de 5 min as 3h some na media de 8h).
        if (!incidents.empty())
        {
            const Incident* worst = &incidents.front();
            long long total = 0;
            for (const auto& inc : incidents)
            {
                total += inc.durationSeconds;
                if (inc.durationSeconds > worst->durationSeconds)
                {
                    worst = &inc;
                }
            }
            const bool severe = worst->durationSeconds >= 120 || incidents.size() >= 3 || total >= 300;
            const Level level = severe ? Level::Bottleneck : Level::Warning;
            const std::string headline =
                "Foram detectados " + std::to_string(incidents.size()) + " incidente(s) durante o periodo. "
                "O mais longo: " + worst->resource + " comecando " + ShortTime(worst->startTs) +
                " por " + TimeHelper::FormatDuration(worst->durationSeconds) + " (pico " +
                FormatNum(worst->peak) + "%), provavel culpado " + worst->culprit +
                ". Veja a linha do tempo abaixo para todos os momentos.";
            return {level, worst->resource, headline, RecommendFor(worst->resource), evidence};
        }

        // 3) Sem incidentes: avaliacao graduada pelas medias/picos do periodo.
        const std::vector<Resource> resources = {
            EvalCpu(st), EvalRam(st), EvalDiskSpace(st), EvalDiskActivity(st)};

        const Resource* primary = &resources[0];
        Level worst = Level::Healthy;
        for (const auto& r : resources)
        {
            if (static_cast<int>(r.level) > static_cast<int>(worst))
            {
                worst = r.level;
            }
        }
        for (const auto& r : resources)
        {
            if (r.level == worst && r.pressure > primary->pressure)
            {
                primary = &r;
            }
        }
        if (worst == Level::Healthy)
        {
            for (const auto& r : resources)
            {
                if (r.pressure > primary->pressure)
                {
                    primary = &r;
                }
            }
        }

        const std::string dur = TimeHelper::FormatDuration(durationSeconds);
        std::string headline;
        if (worst == Level::Healthy)
        {
            headline = "Durante " + dur + " de monitoramento, a maquina se manteve SAUDAVEL: "
                       "nenhum recurso ficou sob pressao relevante e nenhum incidente sustentado "
                       "ocorreu. O mais solicitado foi " + primary->name + " (" + primary->detail +
                       "), dentro do normal. Se voce sente lentidao mas nada apareceu, ela e' "
                       "provavelmente INTERMITENTE: rode durante o momento de lentidao ou aumente a duracao.";
        }
        else if (worst == Level::Warning)
        {
            headline = "Estado de ATENCAO. O ponto mais pressionado foi " + primary->name +
                       ": " + primary->detail + ". Nao houve pico sustentado, mas vale acompanhar.";
        }
        else
        {
            headline = "GARGALO PROVAVEL: " + primary->name + " - " + primary->detail +
                       ". Esse recurso atingiu niveis que costumam causar lentidao perceptivel.";
        }

        return {worst, primary->name, headline, RecommendFor(primary->name), evidence};
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
        const double highIoPercent = Pct(st.ioHigh, st.count);

        const std::string spaceStatus =
            (st.freePercent < 5 || st.diskPeak >= 98) ? "CRITICO"
            : (st.freePercent < 12 || st.diskPeak >= 90) ? "ATENCAO"
                                                         : "OK";

        std::string ioStatus;
        if (st.hasActive)
        {
            ioStatus = (st.activeAvg >= 80) ? "ALTO" : (st.activeAvg >= 50) ? "MODERADO" : "NORMAL";
        }
        else
        {
            ioStatus = (ioAvg >= 50 || st.ioPeak >= 150 || highIoPercent >= 20) ? "ALTO"
                     : (ioAvg >= 20 || st.ioPeak >= 80 || highIoPercent >= 10) ? "MODERADO"
                                                                               : "NORMAL";
        }

        out << "Validacao rapida do disco:\n";
        out << "- Espaco ocupado: " << spaceStatus << " - pico " << FormatNum(st.diskPeak)
            << "% usado, " << FormatNum(st.freePercent) << "% livre.\n";
        out << "- Atividade de disco: " << ioStatus << " - media " << FormatNum(ioAvg)
            << " MB/s, pico " << FormatNum(st.ioPeak) << " MB/s.\n";
        if (st.hasActive)
        {
            out << "- Tempo de disco ocupado: media " << FormatNum(st.activeAvg)
                << "%, pico " << FormatNum(st.activePeak) << "% (proximo de 100% = disco saturado).\n";
        }
        out << "Observacao: espaco ocupado e atividade de disco sao coisas diferentes; "
               "o CSV guarda ambos separadamente.\n\n";
    }

    std::string BuildReport(const MonitorSummary& summary)
    {
        const Stats st = Compute(summary.samples);
        const std::vector<ProcessGroupSample> grouped = GroupProcesses(summary.processSamples);
        const std::vector<Incident> incidents =
            DetectIncidents(summary.samples, summary.processSamples,
                            summary.intervalSeconds > 0 ? summary.intervalSeconds : 1,
                            summary.incident);
        const long long durationSeconds =
            static_cast<long long>(summary.finishedAt - summary.startedAt);
        const Verdict verdict = Analyze(st, grouped, summary.smart, incidents, durationSeconds);

        std::ostringstream out;
        out << "SystemMonitorLogger - Relatorio de Diagnostico\n";
        out << "==============================================\n\n";

        out << "VEREDITO: " << LevelLabel(verdict.level) << "\n";
        out << verdict.headline << "\n\n";

        out << "Computador: " << summary.machineName << "\n";
        out << "Sistema: " << summary.operatingSystem << "\n";
        out << "Inicio: " << TimeHelper::FormatDateTime(summary.startedAt) << "\n";
        out << "Fim: " << TimeHelper::FormatDateTime(summary.finishedAt) << "\n";
        out << "Duracao: " << TimeHelper::FormatDuration(durationSeconds) << "\n";
        out << "Intervalo: " << summary.intervalSeconds << " segundos\n";
        out << "Amostras: " << st.count << "\n\n";

        if (st.count == 0)
        {
            out << "Nenhuma amostra foi coletada.\n";
            return out.str();
        }

        // Linha do tempo logo apos o veredito: e o que mais importa num teste longo.
        AppendTimeline(out, incidents, summary.incident.minSeconds);
        AppendHourly(out, summary.samples);
        AppendTrend(out, summary.samples, summary.processSamples);

        out << "Resumo do periodo:\n";
        AppendMetric(out, "CPU", st.cpuAvg, st.cpuPeak);
        AppendMetric(out, "RAM", st.memAvg, st.memPeak);
        AppendMetric(out, "Espaco em disco", st.diskAvg, st.diskPeak);
        out << "Leitura de disco media: " << FormatNum(st.readAvg) << " MB/s (pico "
            << FormatNum(st.readPeak) << " MB/s)\n";
        out << "Escrita de disco media: " << FormatNum(st.writeAvg) << " MB/s (pico "
            << FormatNum(st.writePeak) << " MB/s)\n";
        if (st.hasActive)
        {
            out << "Tempo de disco ocupado medio: " << FormatNum(st.activeAvg) << "% (pico "
                << FormatNum(st.activePeak) << "%)\n";
        }
        out << "Espaco livre em disco: " << FormatNum(st.lastFreeMb / 1024.0) << " GB de "
            << FormatNum(st.lastTotalMb / 1024.0) << " GB\n\n";

        AppendDiskValidation(out, st);
        AppendProcessSummary(out, grouped);
        AppendSmartSummary(out, summary.smart);

        out << "\nDiagnostico:\n";
        out << "Nivel: " << LevelLabel(verdict.level) << "\n";
        out << "Recurso mais critico: " << verdict.primary << "\n\n";
        out << "Evidencias:\n";
        for (const auto& e : verdict.evidence)
        {
            out << "- " << e << "\n";
        }

        out << "\nRecomendacoes:\n";
        int item = 1;
        for (const auto& r : verdict.recommendations)
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
