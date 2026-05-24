# Arquitetura

O SystemMonitorLogger é uma Console App em .NET 8 com estrutura simples e sem dependências complexas.

---

## Organização dos arquivos

| Arquivo / Pasta | Responsabilidade |
|-----------------|-----------------|
| `Program.cs` | Ponto de entrada: argumentos, permissões, seleção de plataforma e encerramento |
| `AppSettings.cs` | Configurações da execução (duração, intervalo, SMART, modo) |
| `Services/InteractiveSettingsService.cs` | Assistente interativo via Spectre.Console quando nenhum parâmetro é informado |
| `Services/MonitorService.cs` | Loop principal de coleta e atualização de tela |
| `Services/ProcessService.cs` | Coleta de top processos por CPU e memória |
| `Services/CsvLogService.cs` | Gravação de `samples.csv` e `processes.csv` |
| `Services/ReportService.cs` | Geração do relatório final com diagnóstico e recomendações |
| `Services/SmartService.cs` | Coleta SMART via `smartctl` embutido ou instalado no sistema |
| `Platform/ISystemMetricsProvider.cs` | Interface comum para coleta de métricas por plataforma |
| `Platform/WindowsMetricsProvider.cs` | Implementação para Windows 10/11 |
| `Platform/LinuxMetricsProvider.cs` | Implementação para Linux |
| `Models/` | Modelos de dados das amostras e resumo |
| `Utils/` | Utilitários (permissões, tempo, arquivos) |
| `Resources/smartctl/` | Binários `smartctl` embutidos para Windows e Linux |

---

## Fluxo de execução

```
1. Program.cs interpreta os argumentos e cria a pasta da execução em ./logs
2. Se nenhum parâmetro foi passado → InteractiveSettingsService pergunta as configurações
3. SmartService tenta coletar SMART no início (sem interromper em caso de falha)
4. MonitorService inicia o loop: coleta CPU, RAM, espaço em disco e I/O a cada 1 segundo
5. ProcessService coleta top processos a cada 5 segundos
6. CsvLogService grava samples.csv e processes.csv
7. Ao encerrar (CTRL+C ou duração atingida), ReportService gera report.txt
```

---

## Interface de terminal

- **Sem parâmetros:** assistente interativo do Spectre.Console
- **Com parâmetros:** execução direta (útil para automação)
- **Modo padrão:** `LiveDisplay` — atualiza a tela sem limpá-la inteiramente a cada ciclo
- **`--simple`:** saída textual compacta para terminais limitados
- **Fallback automático:** se `LiveDisplay` falhar, o monitoramento continua em modo simples

### Dashboard

O dashboard mostra `Top CPU` e `Top RAM` separadamente para não confundir processos com alto uso de memória com os de alto uso de CPU.

Processos com o mesmo nome amigável são agrupados para aproximar a leitura do Gerenciador de Tarefas. O nome amigável é lido dos metadados do executável quando disponível (ex: `Visual Studio Code` em vez de `Code`). No Linux, tenta melhorar o nome via `/proc/<pid>/cmdline`.

O `processes.csv` mantém cada PID individual para análise técnica detalhada.

---

## I/O de disco

- **Windows:** usa `DeviceIoControl` para consultar bytes lidos/escritos do disco lógico principal
- **Linux:** lê `/proc/diskstats` e calcula o delta de setores entre amostras
- Se os dados não estiverem disponíveis, os campos ficam como `0` e o monitoramento continua normalmente

---

## SMART

```
Resources/smartctl/windows/smartctl.exe   ← smartmontools 7.5
Resources/smartctl/linux/smartctl         ← smartmontools 7.4 (Ubuntu Noble amd64)
```

Os binários são incluídos como `EmbeddedResource` no `.csproj` quando existem. Durante a execução, são extraídos para uma pasta temporária e removidos ao finalizar.

Se os binários não existirem no projeto, o aplicativo tenta usar `smartctl` instalado no `PATH` como fallback.
