# SystemMonitorLogger

Ferramenta de terminal em C#/.NET para **monitorar CPU, memória, disco e processos**, gerando logs e relatórios simples para diagnóstico de lentidão em computadores de trabalho.

![Windows 10/11](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows)
![Linux](https://img.shields.io/badge/Linux-amd64-FCC624?logo=linux&logoColor=black)
![.NET 8](https://img.shields.io/badge/.NET-8-512BD4?logo=dotnet)
![License MIT](https://img.shields.io/badge/License-MIT-yellow)

---

## 🆕 Versão C++ (Linux 32/64 e Windows 32/64) — v2.0.0

A pasta [`SystemMonitorLoggerCpp/`](SystemMonitorLoggerCpp/) traz uma **reescrita completa em
C++17**, criada para rodar em **Linux 32-bit (Lubuntu antigo)** — algo que o .NET **não** suporta —
mantendo também Linux 64-bit e Windows 32/64-bit. Mesma funcionalidade (CPU, RAM, disco, I/O,
processos e SMART), mesmos arquivos de saída.

### Como funciona agora

- **4 binários, um por sistema**, cada um **autossuficiente** (linkagem 100% estática — sem
  instalar nada, sem DLL, sem .NET, sem problema de versão de glibc em Lubuntu antigo).
- **Assistente interativo:** ao abrir **sem argumentos**, ele **pergunta** duração, intervalo,
  SMART e modo de tela — não precisa decorar comando nenhum. Com argumentos, inicia direto.
- **Tela ao vivo** com barras de CPU/RAM/disco e top processos por CPU e por RAM, atualizando
  sem piscar (e compatível com o console do Windows 7).

| Sistema | Arquivo (na release) |
|---------|----------------------|
| Linux 32-bit (Lubuntu antigo) | `SystemMonitorLogger-linux-x86` |
| Linux 64-bit | `SystemMonitorLogger-linux-x64` |
| Windows 32-bit | `SystemMonitorLogger-win-x86.exe` |
| Windows 64-bit | `SystemMonitorLogger-win-x64.exe` |

### Uso rápido

```bash
# Linux: 1 arquivo, dá permissão e roda (abre o assistente)
chmod +x SystemMonitorLogger-linux-x86
./SystemMonitorLogger-linux-x86

# Ou direto, sem assistente:
./SystemMonitorLogger-linux-x86 --duration 30m --interval 5 --simple
```

No Windows é só dar duplo-clique no `.exe`.

### Como é compilado

Os 4 binários são gerados **a partir de uma máquina Windows usando apenas Docker** (sem instalar
compiladores): `g++`/`-m32` para Linux e **MinGW-w64** para Windows, tudo num container. Basta:

```powershell
cd SystemMonitorLoggerCpp
.\build_all.ps1
```

Detalhes em [`SystemMonitorLoggerCpp/README.md`](SystemMonitorLoggerCpp/README.md) e
[`SystemMonitorLoggerCpp/README-LINUX.md`](SystemMonitorLoggerCpp/README-LINUX.md).

> A versão **C#** abaixo continua disponível como referência para Windows/Linux 64-bit.

---

## Funcionalidades

- Monitora **CPU, RAM e disco** com amostras a cada 1 segundo
- Registra **top processos** por CPU e memória a cada 5 segundos
- Grava **saúde física do disco** (SMART) usando `smartctl` embutido
- Gera relatório final em `report.txt` com diagnóstico e recomendações
- Exporta amostras em `samples.csv` e `processes.csv` para análise posterior
- Interface com **dashboard dinâmico** via Spectre.Console ou modo simples para terminais limitados
- Funciona em **Windows 10, Windows 11 e Linux** como executável único sem dependências

---

## Download

Baixe a versão mais recente em:

[Releases](https://github.com/c-Morette/system-monitor-logger/releases/latest)

| Sistema | Arquivo |
|---------|---------|
| Windows x64 | `SystemMonitorLogger-win-x64.exe` |
| Linux x64 | `SystemMonitorLogger-linux-x64` |

Para Linux, leia também o arquivo `README-LINUX.md` disponível na mesma release.

---

## Uso rápido

Sem parâmetros, abre um assistente interativo no terminal:

**Windows:**

```powershell
.\SystemMonitorLogger-win-x64.exe
```

**Linux:**

```bash
chmod +x SystemMonitorLogger-linux-x64
./SystemMonitorLogger-linux-x64
```

O assistente pergunta duração, intervalo, verificação SMART e modo de tela.

Com parâmetros, inicia direto:

```powershell
.\SystemMonitorLogger-win-x64.exe --duration 30m
.\SystemMonitorLogger-win-x64.exe --duration 1h30m --interval 5
.\SystemMonitorLogger-win-x64.exe --duration 1h --no-smart --simple
```

No Linux, use o mesmo padrão trocando pelo executável Linux:

```bash
./SystemMonitorLogger-linux-x64 --duration 30m
./SystemMonitorLogger-linux-x64 --duration 1h30m --interval 5
./SystemMonitorLogger-linux-x64 --duration 1h --no-smart --simple
```

---

## Parâmetros

| Parâmetro | Descrição | Exemplo |
|-----------|-----------|---------|
| `--duration` | Duração do monitoramento | `30m`, `1h`, `1h30m`, `notime` |
| `--interval` | Intervalo de coleta em segundos (padrão: `1`) | `5` |
| `--no-smart` | Desativa a verificação SMART | — |
| `--simple` | Usa saída textual simples em vez do dashboard | — |

**Formatos de duração aceitos:**

```
m = minutos   h = horas   notime = sem limite
Exemplos: 5m | 30m | 1h | 1h30m | notime
```

---

## Saídas

Cada execução cria uma subpasta em `./logs` com data, hora, nome do computador e PID:

```
logs/
└── 2026-05-24_18-30-00-123_COMPUTADOR-01_PID1234/
    ├── report.txt       ← relatório final com diagnóstico
    ├── samples.csv      ← amostras do sistema por segundo
    ├── processes.csv    ← top processos por amostragem
    └── smart.txt        ← saúde física do disco (quando disponível)
```

Os arquivos são sempre salvos em `./logs`, ao lado do executável.

### Campos de `samples.csv`

```csv
timestamp,cpu_percent,memory_used_percent,memory_used_mb,memory_total_mb,disk_usage_percent,disk_free_mb,disk_total_mb,disk_read_mb_s,disk_write_mb_s
```

### Campos de `processes.csv`

```csv
timestamp,process_name,display_name,pid,cpu_percent,memory_mb
```

---

## SMART

O `smartctl` está embutido no executável — não é necessário instalação externa.

- Extrai automaticamente para uma pasta temporária durante a execução
- Remove os arquivos temporários ao finalizar
- Se o SMART falhar (sem permissão, disco incompatível), o monitoramento continua normalmente

Para melhores resultados, execute como administrador (Windows) ou root (Linux).

Veja [docs/SMART.md](docs/SMART.md) para detalhes.

---

## Publicação

**Windows x64:**

```bash
dotnet publish src/SystemMonitorLogger -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```

**Linux x64:**

```bash
dotnet publish src/SystemMonitorLogger -c Release -r linux-x64 --self-contained true /p:PublishSingleFile=true
```

Ou via `dotnet run` durante o desenvolvimento:

```bash
dotnet run --project src/SystemMonitorLogger -- --duration 5m
```

---

## Estrutura do projeto

```
SystemMonitorLogger/
├── src/
│   └── SystemMonitorLogger/
│       ├── Program.cs
│       ├── AppSettings.cs
│       ├── Models/                  # Modelos de dados
│       ├── Services/                # Lógica principal (monitor, CSV, relatório, SMART)
│       ├── Platform/                # Provedores por SO (Windows / Linux)
│       ├── Utils/                   # Utilitários (admin, tempo, arquivos)
│       └── Resources/smartctl/      # Binários smartctl embutidos
│           ├── windows/smartctl.exe
│           └── linux/smartctl
├── docs/
│   ├── ARCHITECTURE.md
│   ├── USAGE.md
│   ├── EXAMPLES.md
│   └── SMART.md
├── logs/                            # Gerado em tempo de execução
├── README.md
├── THIRD_PARTY_NOTICES.md
└── SystemMonitorLogger.sln
```

---

## Requisitos de desenvolvimento

- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
- Windows 10/11 x64 ou Linux x64

---

## Licença

O código-fonte deste projeto é distribuído sob a licença **MIT** — veja o arquivo [LICENSE](LICENSE).

O executável inclui binários do **[smartmontools](https://www.smartmontools.org/)** (`smartctl`), distribuídos sob a
[GNU GPL v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).
Veja [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) para detalhes.
