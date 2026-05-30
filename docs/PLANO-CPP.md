# Plano de Implementação — Reescrita em C++ (SystemMonitorLoggerCpp)

> Documento de planejamento. Nenhum código ainda — este é o roteiro.

## 1. Objetivo

Reescrever o SystemMonitorLogger (hoje C#/.NET 8) em **C++**, para gerar **4 binários** a partir
de uma máquina de desenvolvimento **Windows 11/10 64-bit**:

- Linux 32-bit (i386) — alvo principal: PCs antigos com Lubuntu
- Linux 64-bit
- Windows 32-bit
- Windows 64-bit

## 2. Decisões fechadas

| Tema | Decisão |
|------|---------|
| Linguagem | **C++17** (estilo OOP próximo do Enforce Script), C nas leituras de baixo nível |
| Build dos binários Linux | **Docker** (já instalado; isolado do Postgres `trustban-pg-dev`) |
| Build dos binários Windows | MSVC na máquina **ou** MinGW-w64 em container |
| Dependências externas | **Nenhuma** — só stdlib + POSIX (Linux) / Win32 API (Windows) |
| SMART | Usar `smartctl` do sistema se existir; senão pular graciosamente |
| Interface | Texto ANSI simples (binário enxuto para PC velho) |
| Inteiros | **`uint64_t` / largura fixa** em tudo que é byte/setor (crítico p/ 32-bit) |

## 3. Estrutura de pastas

```
SystemMonitorLoggerCpp/
├── CMakeLists.txt
├── build_all.ps1                 # gera os 4 binários
├── docker/
│   └── Dockerfile.linux          # Ubuntu + g++ + gcc-multilib
├── cmake/
│   ├── win-x86.cmake             # toolchain files (Windows só)
│   └── win-x64.cmake
├── src/
│   ├── main.cpp                  # entrada: args, loop, encerramento
│   ├── models/
│   │   ├── SystemSample.hpp
│   │   ├── ProcessSample.hpp
│   │   ├── SmartResult.hpp
│   │   └── MonitorSummary.hpp
│   ├── platform/
│   │   ├── MetricsProvider.hpp   # classe base abstrata (interface)
│   │   ├── LinuxMetricsProvider.{hpp,cpp}
│   │   └── WindowsMetricsProvider.{hpp,cpp}
│   ├── services/
│   │   ├── MonitorService.{hpp,cpp}
│   │   ├── ProcessService.{hpp,cpp}
│   │   ├── CsvLogService.{hpp,cpp}
│   │   ├── ReportService.{hpp,cpp}
│   │   └── SmartService.{hpp,cpp}
│   └── utils/
│       ├── TimeHelper.{hpp,cpp}
│       └── FileHelper.{hpp,cpp}
└── README-LINUX.md
```

`models/` e `services/` são **código comum** aos 4 binários.
Só `platform/` tem código específico de SO (selecionado por `#ifdef _WIN32`).

## 4. Desenho das classes (vocabulário Enforce Script → C++)

### Classe base (interface) — igual a `ISystemMetricsProvider`

```
// Enforce: class LinuxMetricsProvider extends MetricsProvider { override ... }
class MetricsProvider {                 // classe abstrata
public:
    virtual ~MetricsProvider() = default;
    virtual SystemSample GetSystemSample() = 0;   // método abstrato
};

class LinuxMetricsProvider : public MetricsProvider {
public:
    SystemSample GetSystemSample() override;       // override, como no Enforce
private:
    CpuTimes        m_lastCpu;
    DiskIoSnapshot  m_lastDiskIo;
};
```

`main.cpp` escolhe a implementação em tempo de compilação:

```
#ifdef _WIN32
    auto provider = std::make_unique<WindowsMetricsProvider>();
#else
    auto provider = std::make_unique<LinuxMetricsProvider>();
#endif
```

### Mapa mental Enforce → C++

| Enforce Script | C++ |
|---|---|
| `class A extends B` | `class A : public B` |
| `override void F()` | `void F() override` |
| método abstrato | `virtual void F() = 0;` |
| `array<ref ProcessSample>` | `std::vector<ProcessSample>` |
| `ref X` | `std::unique_ptr` / `std::shared_ptr` / valor (RAII) |
| `string` | `std::string` |
| `map<string,float>` | `std::unordered_map<std::string,double>` |

## 5. Camada de plataforma — o que cada SO usa

| Métrica | Linux (já mapeado do C# atual) | Windows |
|---|---|---|
| CPU % | `/proc/stat` (delta busy/idle) | `GetSystemTimes()` |
| RAM | `/proc/meminfo` | `GlobalMemoryStatusEx()` |
| Disco (espaço) | `statvfs()` | `GetDiskFreeSpaceEx()` |
| Disco I/O | `/proc/diskstats` (delta setores ×512) | `DeviceIoControl` ou PDH |
| Processos | varrer `/proc/<pid>/stat` e `/cmdline` | `CreateToolhelp32Snapshot` + `GetProcessTimes` + `GetProcessMemoryInfo` |
| Nome amigável | `/proc/<pid>/cmdline` | `GetFileVersionInfo` |

A lógica de cálculo (delta de CPU, MB/s, agrupamento de processos) é aritmética pura,
copiada conceitualmente do projeto C# atual.

## 6. Models (structs simples)

Campos idênticos ao CSV atual para manter compatibilidade dos relatórios:

```
struct SystemSample {
    std::string timestamp;
    double cpuPercent;
    double memoryUsedPercent, memoryUsedMb, memoryTotalMb;
    double diskUsagePercent, diskFreeMb, diskTotalMb;
    double diskReadMbPerSecond, diskWriteMbPerSecond;
};
struct ProcessSample {
    std::string timestamp, processName, displayName;
    uint32_t pid;
    double cpuPercent, memoryMb;
};
```

## 7. Services

| Service | Responsabilidade |
|---|---|
| `MonitorService` | Loop principal: coleta a cada N segundos, atualiza tela, respeita duração/CTRL+C |
| `ProcessService` | Top processos por CPU e memória (delta de CPU entre amostras) |
| `CsvLogService` | Grava `samples.csv` e `processes.csv` |
| `ReportService` | Gera `report.txt` com diagnóstico e recomendações |
| `SmartService` | Chama `smartctl` do sistema; salva `smart.txt`; pula se ausente |

Saída idêntica à atual: pasta `logs/<data>_<host>_PID<n>/` com os 4 arquivos.

## 8. Build

### Dockerfile.linux (conceito)
```
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y g++ gcc-multilib g++-multilib cmake
# build x64:  cmake + make
# build x86:  CXXFLAGS="-m32" cmake + make
# saída estática recomendada: -static-libstdc++ -static-libgcc
```

### build_all.ps1 (conceito)
1. `docker build` da imagem de build Linux (uma vez).
2. `docker run` montando o projeto → produz `SystemMonitorLogger-linux-x64` e `-linux-x86`.
3. MSVC/MinGW na máquina → `SystemMonitorLogger-win-x64.exe` e `-win-x86.exe`.
4. Copia os 4 binários para `dist/`.

Docker é isolado: não toca no container Postgres nem nos volumes de banco.

## 9. Ordem de trabalho (marcos incrementais)

Cada fase entrega algo **compilável e testável**; nada de big bang.

- **Fase 0 — Esqueleto + build. ✅ CONCLUÍDA.** `main.cpp` "hello", `CMakeLists.txt`,
  `Dockerfile.linux` (Ubuntu + g++ + multilib + MinGW-w64), `docker/build.sh`, `build_all.ps1`.
  Os **4 binários** (linux-x64/x86, win-x64/x86) saem da máquina Windows só com Docker e foram
  executados com sucesso. MinGW-w64 cross-compila os `.exe` — não precisa MSVC.
- **Fase 1 — Métricas Linux. ✅ CONCLUÍDA (falta validar em Lubuntu real).**
  `MetricsProvider` (base abstrata) + `LinuxMetricsProvider` lendo `/proc/stat`, `/proc/meminfo`,
  `statvfs("/")` e `/proc/diskstats`. `models/SystemSample`, `utils/TimeHelper` (NowString + SleepMs
  portátil — MinGW win32-threads não tem `std::this_thread`). Stub `WindowsMetricsProvider` (zeros).
  Testado nos 4 binários no Docker: CPU/RAM/IO corretos, 32-bit OK. **Pendente: rodar num Lubuntu 32-bit físico.**
- **Fase 2 — CSV + Report. ✅ CONCLUÍDA.** `CsvLogService` (`samples.csv`), `ReportService`
  (`report.txt` com resumo, validação de disco e diagnóstico CPU/RAM/disco com scoring portado do C#),
  `FileHelper` (pasta `logs/<data>_<host>_PID<n>/`, hostname, PID, SO), `TimeHelper` (data/duração),
  `utils/Format` (`0.##`), `models/MonitorSummary`. Validado: dados reais no Linux 32/64; disco de ~1 TB
  lido certo no 32-bit graças a **`_FILE_OFFSET_BITS=64`** (statvfs64). Seções de processos/SMART ficam p/ Fases 3/4.
- **Fase 3 — Processos. ✅ CONCLUÍDA.** Abstração `ProcessProvider` + `LinuxProcessProvider`
  (`/proc/<pid>/stat` p/ CPU via delta de ticks, `statm` p/ RSS, `comm`/`cmdline` p/ nome) +
  stub `WindowsProcessProvider`. `ProcessService::SelectTop` (lógica comum). `processes.csv` e
  seção de processos no relatório (agrupamento por nome amigável, top CPU/RAM, evidências).
  Validado: `md5sum` agrupado somando 2 PIDs; nome completo via cmdline (comm trunca em 16 chars).
- **Fase 4 — SMART. ✅ CONCLUÍDA.** `SmartService` chama o `smartctl` do sistema (PATH) via
  `popen`/`_popen`: `--version` (disponibilidade), `--scan-open` (device), `-H -A` (saúde+atributos).
  Parsing line-based (health PASSED/FAILED, temperatura, setores realocados/pendentes, horas).
  `smart.txt` + seção SMART no relatório + **prioridade do SMART no diagnóstico**. `models/SmartResult`
  (campos `std::optional`). Fallback gracioso se ausente. Validado: fallback e parse (smartctl falso FAILED).
  **Nunca embarca binário** (decisão do projeto).
- **Fase 5 — Camada Windows. ✅ CONCLUÍDA.** `WindowsMetricsProvider` (Win32: `GetSystemTimes`,
  `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`, `DeviceIoControl`+`IOCTL_DISK_PERFORMANCE`) e
  `WindowsProcessProvider` (`CreateToolhelp32Snapshot`, `GetProcessTimes`, `GetProcessMemoryInfo`,
  nome amigável via `GetFileVersionInfo`). CMake linka `psapi` e `version`; baseline `_WIN32_WINNT=0x0601`.
  **Validado nativamente nesta máquina**: win-x64 (330 procs, nomes tipo "Steam Client WebHelper") e
  win-x86 (32 GB RAM lidos certo no processo 32-bit, 326 procs, I/O real). Os 4 alvos agora são funcionais.
- **Fase 6 — Interface + args. ✅ CONCLUÍDA.** `AppSettings` + `utils/Args` (parsing de
  `--duration` [30m/1h/1h30m/90s/notime], `--interval`, `--no-smart`, `--simple`, `--help`, com erros).
  `MonitorService` (loop real: duração ou CTRL+C, processos a cada ~5s, sleep em fatias p/ responder ao sinal).
  `utils/Console` com redraw **Win7-safe** (API de console no Windows — sem ANSI — e ANSI no Linux).
  Dashboard ao vivo (barras + top processos) e `--simple`. CTRL+C via `std::signal`→`RequestStop` (lógica pronta;
  validar interativamente). Validado: args, modo simples e ao vivo no Win e Linux.
- **Fase 7 — Polimento. ✅ (parte de código/docs).** Linkagem **100% estática** nos 4 alvos
  (`-static` no Linux também → resolve glibc dos Lubuntu antigos; `ldd` = "not a dynamic executable").
  Verificado: win-x64 importa só KERNEL32/msvcrt/VERSION. `README-LINUX.md` criado; README raiz
  menciona a versão C++. **Pendente (externo, não dá para fazer aqui):** rodar em Lubuntu 32 físico
  vs `htop`, rodar em Win7 físico, e validar CTRL+C interativamente.

## 10. Pontos de atenção

1. **`uint64_t` em bytes/setores** — `long` é 32-bit no i386 e estoura > 4 GB. Bug nº 1 de port 32-bit.
2. **glibc** — se os Lubuntu forem antigos, linkar estático (`-static-libstdc++ -static-libgcc`)
   ou (se usar Zig no futuro) fixar versão da glibc.
3. **SMART** — nunca embarcar binário; depender do sistema e pular se ausente.
4. **UI** — texto ANSI puro evita `#ifdef` de UI e mantém o binário leve.

## 11. Validação

- Testar Fase 1 num Lubuntu 32-bit **real** (não só no Docker) — confirma glibc e leitura de `/proc`.
- Comparar números (CPU/RAM) com `htop`/`top` para validar os cálculos.
- Conferir que os CSVs abrem igual aos do projeto C# (mesmos cabeçalhos).
