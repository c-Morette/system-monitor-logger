# SystemMonitorLogger (C++)

Reescrita em **C++17** do SystemMonitorLogger (originalmente C#/.NET 8), para rodar em
**Linux 32/64-bit e Windows 32/64-bit**. Motivo principal: os PCs antigos de trabalho usam
**Lubuntu 32-bit**, e o .NET não tem runtime para Linux 32-bit.

Monitora **CPU, RAM, disco e processos**, gera logs/relatórios — sem dependências externas
(só stdlib + POSIX no Linux / Win32 API no Windows).

## Pré-requisito

Apenas **Docker**. Toda a compilação dos 4 binários acontece dentro de um container Ubuntu —
nada precisa estar instalado na máquina (nem cmake, nem Visual Studio, nem MinGW).

## Como compilar

```powershell
# Os 4 alvos (linux-x64, linux-x86, win-x64, win-x86):
.\build_all.ps1

# Apenas alvos específicos:
.\build_all.ps1 -Targets linux-x86
.\build_all.ps1 -Targets win-x64,win-x86
```

Os binários saem em `dist/`:

| Alvo | Arquivo | Compilador (no container) |
|------|---------|---------------------------|
| Linux 64 | `SystemMonitorLogger-linux-x64` | `g++` nativo |
| Linux 32 | `SystemMonitorLogger-linux-x86` | `g++ -m32` (gcc-multilib) |
| Windows 64 | `SystemMonitorLogger-win-x64.exe` | `x86_64-w64-mingw32-g++` (MinGW-w64) |
| Windows 32 | `SystemMonitorLogger-win-x86.exe` | `i686-w64-mingw32-g++` (MinGW-w64) |

Tudo é linkado **estaticamente** (`-static-libstdc++ -static-libgcc`, e `-static` no MinGW)
para rodar em Lubuntu antigos e em Windows sem DLLs do runtime ao lado.

## Uso

**Sem argumentos**, abre um **assistente interativo** que pergunta duração, intervalo,
SMART e modo (basta apertar ENTER para aceitar os padrões) — não precisa decorar comandos.

**Com argumentos**, inicia direto (útil para automação):

```
SystemMonitorLogger [opcoes]
  --duration <valor>  30m, 1h, 1h30m, 90s, notime   (padrao: notime = ate CTRL+C)
  --interval <n>      intervalo de coleta em segundos (padrao: 1)
  --no-smart          desativa a verificacao SMART
  --simple            saida textual simples (sem tela ao vivo)
  --quiet             modo silencioso: nao desenha nada na tela (so grava logs)
  --report-every <n>  regrava o relatorio a cada n minutos (padrao: 5; 0 desliga)
  --sensitivity <p>   sensibilidade dos incidentes: baixa | normal | alta (padrao: normal)
  --help, -h          ajuda
```

**Sensibilidade dos incidentes** (`--sensitivity`): controla o que vira "incidente" na
linha do tempo, sem recompilar.

| Preset | Tempo mínimo | CPU | RAM | Disco ativo | Quando usar |
|--------|--------------|-----|-----|-------------|-------------|
| `baixa` | 60s | ≥90% | ≥93% | ≥85% | só picos sérios e longos (menos ruído) |
| `normal` | 30s | ≥85% | ≥90% | ≥80% | padrão equilibrado |
| `alta` | 15s | ≥80% | ≥85% | ≥70% | pega até picos curtos/leves (caçar problema raro) |

A tela ao vivo usa a API de console do Windows (funciona no **Win7**, que nao interpreta
codigos ANSI) e ANSI no Linux. Cada execucao grava `samples.csv`, `processes.csv`,
`smart.txt` e `report.txt` em `logs/<data>_<host>_PID<n>/`.

### Diagnóstico desacompanhado (ex.: PDV problemático a noite)

Cenário-alvo: deixar a máquina rodando a noite toda e, de manhã, ler **um relatório que conta
a história** — sem ter ficado olhando a tela.

```powershell
# Roda 8h em modo silencioso (nada na tela), regravando o relatorio a cada 5 min:
.\SystemMonitorLogger-win-x64.exe --duration 8h --quiet

# Caçando um problema raro/intermitente? aumente a sensibilidade:
.\SystemMonitorLogger-win-x64.exe --duration 8h --quiet --sensitivity alta
```

O `report.txt` final traz, além das médias/picos:

- **Veredito no topo** — `SAUDAVEL` / `ATENCAO` / `GARGALO PROVAVEL`, em uma frase.
- **Linha do tempo de incidentes** — cada episódio em que CPU/RAM/disco passou do limite por
  ≥ 30s, com **horário de início/fim, duração, pico e o processo culpado naquele momento**.
  (É o que permite descobrir "às 03h17 o processo X travou a CPU por 4 min" numa noite inteira.)
- **Resumo por hora** — tabela hora-a-hora para localizar *quando* a máquina piorou.
- **Tendência de RAM** — detecta memória subindo sem voltar (vazamento) e aponta o processo.
- **% de tempo de disco ocupado** (`% Disk Time` no Windows via PDH / `%util` no Linux) — o
  indicador que acusa **HD mecânico saturado** mesmo com pouca vazão em MB/s.

O **relatório parcial** (regravado a cada `--report-every` minutos) garante que, se faltar luz
ou a máquina travar, você ainda tem o diagnóstico da noite até o último ponto salvo.

## Estrutura

```
SystemMonitorLoggerCpp/
├── CMakeLists.txt              # C++17, linkagem estática
├── build_all.ps1              # gera os 4 binários via Docker
├── docker/
│   ├── Dockerfile.linux       # Ubuntu + g++ + multilib + MinGW-w64
│   └── build.sh               # compila 1 alvo dentro do container
├── cmake/
│   ├── mingw-w64-x86_64.cmake # toolchain cross Windows 64
│   └── mingw-w64-i686.cmake   # toolchain cross Windows 32
└── src/
    ├── main.cpp
    ├── models/                # structs comuns (SystemSample, ...)
    ├── platform/              # ÚNICO código por-SO
    │   ├── MetricsProvider.hpp        # classe base abstrata (interface)
    │   ├── LinuxMetricsProvider.*     # /proc + statvfs
    │   └── WindowsMetricsProvider.*   # Win32 API
    └── utils/                 # TimeHelper (NowString, SleepMs portátil)
```

`models/` e `services/` (futuro) são código comum aos 4 binários. Só `platform/` tem
código específico de SO, selecionado por `#ifdef _WIN32`.

## Arquitetura (OOP)

`MetricsProvider` é uma classe base abstrata; `LinuxMetricsProvider` e `WindowsMetricsProvider`
a herdam e fazem `override` de `GetSystemSample()`. `main.cpp` instancia a implementação certa
em tempo de compilação. (Espelha a `ISystemMetricsProvider` do projeto C#.)

## Notas técnicas importantes

- **`uint64_t` / largura fixa** em tudo que é byte/setor — `long` é 32-bit no i386 e estoura > 4 GB.
- **MinGW (Ubuntu) usa threads modelo win32** → não há `std::thread`/`std::this_thread`.
  Por isso o loop é sequencial e o sleep é o `TimeHelper::SleepMs` portátil.
- **SMART** (futuro): usar `smartctl` do sistema; nunca embarcar binário.

## Estado / Roteiro

Plano completo em [`../docs/PLANO-CPP.md`](../docs/PLANO-CPP.md).

- [x] **Fase 0** — Esqueleto + build dos 4 alvos via Docker
- [x] **Fase 1** — Métricas Linux (CPU/RAM/disco/IO) via `/proc` + `statvfs`
- [x] **Fase 2** — CSV (`samples.csv`) + Report (`report.txt`) em `logs/<data>_<host>_PID<n>/`
- [x] **Fase 3** — Processos (`/proc/<pid>`) + `processes.csv` + seção no relatório
- [x] **Fase 4** — SMART via `smartctl` do sistema (`smart.txt` + seção/prioridade no relatório)
- [x] **Fase 5** — Camada Windows real (Win32 API) — os 4 alvos funcionais nativamente
- [x] **Fase 6** — Interface ao vivo (Win7-safe) + argumentos (`--duration/--interval/--no-smart/--simple`)
- [x] **Fase 7** — Polimento: linkagem 100% estática (resolve glibc antiga), `README-LINUX.md`
- [x] **v2.1.0** — Disco via PDH (Win7→11) + `% Disk Time`, relatório com **linha do tempo de
      incidentes** (horário/duração/pico/culpado), resumo por hora, vazamento de RAM, veredito
      sempre conclusivo, `--quiet`, `--report-every`, `--sensitivity`. Publicado na release v2.1.0.

> **Teste pendente (segunda-feira, 2026-06-15):** validar nos PDVs reais — ver o
> **plano de teste em [`../docs/TESTE-PDV.md`](../docs/TESTE-PDV.md)**. Pontos-chave: disco via
> PDH no **Win7** (antes zerava), `% de tempo de disco ocupado` num **HD mecânico saturado**, e
> o relatório de incidentes após deixar rodando desacompanhado. Também ainda falta comparar
> `linux-x86` com `htop` num **Lubuntu 32-bit físico**.
