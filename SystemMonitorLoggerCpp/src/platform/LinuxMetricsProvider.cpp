// Implementacao so faz sentido no Linux. Em outros alvos o arquivo compila vazio.
#if !defined(_WIN32)

#include "platform/LinuxMetricsProvider.hpp"
#include "utils/TimeHelper.hpp"

#include <sys/statvfs.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // Quebra uma linha em campos separados por espacos (ignora vazios).
    std::vector<std::string> SplitWhitespace(const std::string& line)
    {
        std::vector<std::string> parts;
        std::istringstream stream(line);
        std::string token;
        while (stream >> token)
        {
            parts.push_back(token);
        }
        return parts;
    }

    std::uint64_t ParseU64(const std::string& value)
    {
        try
        {
            return static_cast<std::uint64_t>(std::stoull(value));
        }
        catch (...)
        {
            return 0;
        }
    }

    // Filtra discos fisicos em /proc/diskstats (mesma regra do projeto C#).
    bool IsPhysicalDisk(const std::string& name)
    {
        auto startsWith = [&](const char* prefix) {
            return name.rfind(prefix, 0) == 0;
        };

        if (startsWith("loop") || startsWith("ram") || startsWith("dm-") || startsWith("md"))
        {
            return false;
        }

        if ((startsWith("sd") || startsWith("vd")) && name.size() == 3)
        {
            return true;
        }

        if (startsWith("xvd") && name.size() == 4)
        {
            return true;
        }

        if (startsWith("nvme") && name.size() >= 2 &&
            name.compare(name.size() - 2, 2, "n1") == 0)
        {
            return true;
        }

        return false;
    }
}

double LinuxMetricsProvider::GetCpuPercent()
{
    std::ifstream stat("/proc/stat");
    std::string line;
    if (!std::getline(stat, line))
    {
        return 0.0;
    }

    const auto parts = SplitWhitespace(line);
    if (parts.size() < 8 || parts[0] != "cpu")
    {
        return 0.0;
    }

    const std::uint64_t user = ParseU64(parts[1]);
    const std::uint64_t nice = ParseU64(parts[2]);
    const std::uint64_t system = ParseU64(parts[3]);
    const std::uint64_t idle = ParseU64(parts[4]);
    const std::uint64_t iowait = ParseU64(parts[5]);
    const std::uint64_t irq = ParseU64(parts[6]);
    const std::uint64_t softirq = ParseU64(parts[7]);
    const std::uint64_t steal = parts.size() > 8 ? ParseU64(parts[8]) : 0;

    const CpuTimes current{user + nice + system + irq + softirq + steal, idle + iowait};

    if (!m_lastCpu)
    {
        m_lastCpu = current;
        return 0.0;
    }

    const CpuTimes previous = *m_lastCpu;
    m_lastCpu = current;

    const std::uint64_t busyDelta = current.busy - previous.busy;
    const std::uint64_t idleDelta = current.idle - previous.idle;
    const std::uint64_t total = busyDelta + idleDelta;

    if (total == 0)
    {
        return 0.0;
    }

    const double percent = static_cast<double>(busyDelta) * 100.0 / static_cast<double>(total);
    return std::clamp(percent, 0.0, 100.0);
}

void LinuxMetricsProvider::GetMemory(double& totalMb, double& usedMb, double& usedPercent)
{
    double totalKb = 0.0;
    double availableKb = 0.0;

    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line))
    {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        const std::string key = line.substr(0, colon);
        const auto numberParts = SplitWhitespace(line.substr(colon + 1));
        if (numberParts.empty())
        {
            continue;
        }

        const double kb = static_cast<double>(ParseU64(numberParts[0]));
        if (key == "MemTotal")
        {
            totalKb = kb;
        }
        else if (key == "MemAvailable")
        {
            availableKb = kb;
        }
    }

    totalMb = totalKb / 1024.0;
    const double availableMb = availableKb / 1024.0;
    usedMb = std::max(0.0, totalMb - availableMb);
    usedPercent = totalMb <= 0.0 ? 0.0 : usedMb * 100.0 / totalMb;
}

void LinuxMetricsProvider::GetDisk(double& totalMb, double& freeMb, double& usedPercent)
{
    struct statvfs vfs{};
    if (statvfs("/", &vfs) != 0)
    {
        totalMb = freeMb = usedPercent = 0.0;
        return;
    }

    const double blockSize = static_cast<double>(vfs.f_frsize);
    totalMb = static_cast<double>(vfs.f_blocks) * blockSize / 1024.0 / 1024.0;
    freeMb = static_cast<double>(vfs.f_bavail) * blockSize / 1024.0 / 1024.0;
    usedPercent = totalMb <= 0.0 ? 0.0 : (totalMb - freeMb) * 100.0 / totalMb;
}

void LinuxMetricsProvider::GetDiskIo(double& readMbPerSecond, double& writeMbPerSecond, double& activePercent)
{
    readMbPerSecond = 0.0;
    writeMbPerSecond = 0.0;
    activePercent = 0.0;

    std::ifstream diskstats("/proc/diskstats");
    if (!diskstats.is_open())
    {
        return;
    }

    std::uint64_t readSectors = 0;
    std::uint64_t writtenSectors = 0;
    // io_ticks (ms ocupado) por disco, para o %util desta amostra.
    std::map<std::string, std::uint64_t> ioTicks;
    std::string line;
    while (std::getline(diskstats, line))
    {
        const auto parts = SplitWhitespace(line);
        if (parts.size() < 14 || !IsPhysicalDisk(parts[2]))
        {
            continue;
        }

        readSectors += ParseU64(parts[5]);
        writtenSectors += ParseU64(parts[9]);
        // Campo 13 (indice 12): "ms doing I/O" = io_ticks, base do %util.
        ioTicks[parts[2]] = ParseU64(parts[12]);
    }

    constexpr std::uint64_t kSectorSize = 512;
    const DiskIoSnapshot current{
        std::chrono::steady_clock::now(),
        readSectors * kSectorSize,
        writtenSectors * kSectorSize};

    const bool hadPrevious = m_lastDiskIo.has_value();
    const DiskIoSnapshot previous = hadPrevious ? *m_lastDiskIo : current;
    m_lastDiskIo = current;

    // %util: maior percentual entre os discos fisicos (o disco mais ocupado).
    // util = delta(io_ticks_ms) / tempo_decorrido_ms * 100.
    const double elapsedSeconds =
        std::chrono::duration<double>(current.timestamp - previous.timestamp).count();
    if (hadPrevious && elapsedSeconds > 0.0)
    {
        const double elapsedMs = elapsedSeconds * 1000.0;
        double maxUtil = 0.0;
        for (const auto& [name, ticks] : ioTicks)
        {
            const auto it = m_lastIoTicks.find(name);
            if (it == m_lastIoTicks.end())
            {
                continue;
            }
            const std::uint64_t deltaMs = ticks >= it->second ? ticks - it->second : 0;
            maxUtil = std::max(maxUtil, static_cast<double>(deltaMs) / elapsedMs * 100.0);
        }
        activePercent = std::clamp(maxUtil, 0.0, 100.0);
    }
    m_lastIoTicks = std::move(ioTicks);

    if (!hadPrevious || elapsedSeconds <= 0.0)
    {
        return;
    }

    const std::uint64_t readDelta =
        current.readBytes >= previous.readBytes ? current.readBytes - previous.readBytes : 0;
    const std::uint64_t writeDelta =
        current.writeBytes >= previous.writeBytes ? current.writeBytes - previous.writeBytes : 0;

    readMbPerSecond = static_cast<double>(readDelta) / 1024.0 / 1024.0 / elapsedSeconds;
    writeMbPerSecond = static_cast<double>(writeDelta) / 1024.0 / 1024.0 / elapsedSeconds;
}

SystemSample LinuxMetricsProvider::GetSystemSample()
{
    SystemSample sample;
    sample.timestamp = TimeHelper::NowString();
    sample.cpuPercent = GetCpuPercent();
    GetMemory(sample.memoryTotalMb, sample.memoryUsedMb, sample.memoryUsedPercent);
    GetDisk(sample.diskTotalMb, sample.diskFreeMb, sample.diskUsagePercent);
    GetDiskIo(sample.diskReadMbPerSecond, sample.diskWriteMbPerSecond, sample.diskActivePercent);
    return sample;
}

#endif // !_WIN32
