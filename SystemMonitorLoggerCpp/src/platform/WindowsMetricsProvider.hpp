#pragma once

#include "platform/MetricsProvider.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

// Coleta de metricas no Windows via Win32 API (kernel32). Equivale ao
// WindowsMetricsProvider.cs do projeto C#.
class WindowsMetricsProvider : public MetricsProvider
{
public:
    SystemSample GetSystemSample() override;

private:
    struct CpuTimes
    {
        std::uint64_t idle = 0;
        std::uint64_t kernel = 0;
        std::uint64_t user = 0;
    };

    struct DiskIoSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        std::uint64_t readBytes = 0;
        std::uint64_t writeBytes = 0;
    };

    std::optional<CpuTimes> m_lastCpu;
    std::optional<DiskIoSnapshot> m_lastDiskIo;

    // Handles do PDH (Performance Data Helper) - o mesmo mecanismo usado pelo
    // Gerenciador de Tarefas. Guardados como void* para nao incluir <pdh.h> no
    // header. Inicializados sob demanda; se o PDH falhar, caimos no IOCTL.
    void* m_pdhQuery = nullptr;    // PDH_HQUERY
    void* m_pdhRead = nullptr;     // PDH_HCOUNTER  (Disk Read Bytes/sec)
    void* m_pdhWrite = nullptr;    // PDH_HCOUNTER  (Disk Write Bytes/sec)
    void* m_pdhActive = nullptr;   // PDH_HCOUNTER  (% Disk Time)
    bool m_pdhTried = false;       // ja tentamos inicializar?
    bool m_pdhReady = false;       // PDH disponivel e funcionando?
    int m_pdhCollections = 0;      // contadores de taxa precisam de >= 2 coletas

    double GetCpuPercent();
    void GetMemory(double& totalMb, double& usedMb, double& usedPercent);
    void GetDisk(double& totalMb, double& freeMb, double& usedPercent);
    // Preenche atividade de disco. activePercent fica 0 quando indisponivel.
    void GetDiskIo(double& readMbPerSecond, double& writeMbPerSecond, double& activePercent);

    bool EnsurePdh();
    bool GetDiskIoPdh(double& readMbPerSecond, double& writeMbPerSecond, double& activePercent);
    void GetDiskIoIoctl(double& readMbPerSecond, double& writeMbPerSecond);

public:
    ~WindowsMetricsProvider() override;
};
