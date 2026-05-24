# SystemMonitorLogger

Ferramenta de terminal em C#/.NET para **monitorar CPU, memória, disco e processos**, gerando logs e relatórios simples para diagnóstico de lentidão em computadores de trabalho.

![Windows 10/11](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows)
![Linux](https://img.shields.io/badge/Linux-amd64-FCC624?logo=linux&logoColor=black)
![.NET 8](https://img.shields.io/badge/.NET-8-512BD4?logo=dotnet)
![License MIT](https://img.shields.io/badge/License-MIT-yellow)

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

## Uso rápido

Sem parâmetros, abre um assistente interativo no terminal:

```bash
SystemMonitorLogger
```

O assistente pergunta duração, intervalo, verificação SMART e modo de tela.

Com parâmetros, inicia direto:

```bash
SystemMonitorLogger --duration 30m
SystemMonitorLogger --duration 1h30m --interval 5
SystemMonitorLogger --duration 1h --no-smart --simple
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
