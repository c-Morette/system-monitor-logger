# Plano de teste nos PDVs — segunda-feira, 2026-06-15

> Lembrete (escrito em 2026-06-14): tudo da **v2.1.0** já foi implementado, validado no PC de
> desenvolvimento (Win11) e **publicado na release v2.1.0**. O que falta é o **teste de campo**
> nas máquinas reais, que só rola na segunda.

## O que baixar

Pegar o binário do sistema do PDV na release **v2.1.0**:
<https://github.com/c-Morette/system-monitor-logger/releases/tag/v2.1.0>

| Sistema do PDV | Arquivo |
|----------------|---------|
| Windows 64-bit (Win7/10/11) | `SystemMonitorLogger-win-x64.exe` |
| Windows 32-bit | `SystemMonitorLogger-win-x86.exe` |
| Lubuntu 32-bit antigo | `SystemMonitorLogger-linux-x86` |
| Linux 64-bit | `SystemMonitorLogger-linux-x64` |

## Comando para deixar rodando (PDV desacompanhado)

```bat
:: A noite toda ou durante o expediente, sem nada na tela:
SystemMonitorLogger-win-x64.exe --duration 8h --quiet

:: Se o problema for raro/intermitente, sobe a sensibilidade:
SystemMonitorLogger-win-x64.exe --duration 8h --quiet --sensitivity alta
```

De manhã (ou na manutenção), abrir `logs/<data>_<host>_PID<n>/report.txt`.

## Checklist do que VALIDAR (foi o motivo da v2.1.0)

- [ ] **Disco no Win7** — antes o I/O zerava (IOCTL no volume lógico). Agora usa **PDH**.
      Confirmar que `Leitura/Escrita de disco` e `% de tempo de disco ocupado` aparecem com
      valores reais no `report.txt` e no `samples.csv` (coluna `disk_active_percent`).
- [ ] **HD mecânico saturado** — num PDV com HD antigo lento, ver se o `% de tempo de disco
      ocupado` chega perto de **100%** quando a máquina engasga. Esse é o número que prova
      "lentidão por causa do disco" (mais do que MB/s).
- [ ] **Linha do tempo de incidentes** — após uma sessão longa, conferir se os momentos de
      lentidão viraram incidentes com **horário, duração, pico e processo culpado** corretos.
- [ ] **Veredito** — o topo do relatório deve concluir algo útil (`SAUDAVEL`/`ATENCAO`/`GARGALO`),
      nunca um "inconclusivo" vazio.
- [ ] **Modo silencioso** — confirmar que com `--quiet` o operador do PDV não vê nada na tela.
- [ ] **Relatório parcial** — se faltar luz/travar, o `report.txt` deve existir com os dados até
      o último ponto salvo (regravado a cada 5 min por padrão).
- [ ] **Lubuntu 32-bit** — rodar `SystemMonitorLogger-linux-x86` e comparar CPU/RAM com o `htop`
      (até agora só testado no Docker, nunca em Lubuntu 32 físico).

## Se algo estiver errado

- Anotar qual máquina (SO/versão, HD ou SSD) e anexar o `report.txt` + `samples.csv` gerados.
- Disco ainda zerando no Win7? O código tem **fallback** para IOCTL; se o PDH falhar, o
  `% de tempo de disco ocupado` fica 0 mas leitura/escrita ainda vêm pelo fallback. Ver
  `SystemMonitorLoggerCpp/src/platform/WindowsMetricsProvider.cpp`.
- Sensibilidade pegando ruído demais (ou de menos): ajustar com `--sensitivity baixa|alta` sem
  recompilar; se precisar de outros limiares, eles ficam em
  `SystemMonitorLoggerCpp/src/models/IncidentConfig.hpp`.

## Estado do código (para retomar)

- Branch `master` atualizada (PR #1 mergeado). Release **v2.1.0** é a Latest.
- Só a **versão C++** recebeu a v2.1.0; a C# segue como referência.
- Build dos 4 binários: `cd SystemMonitorLoggerCpp; .\build_all.ps1` (precisa do Docker Desktop
  rodando). **Não** usar `2>&1` com docker no PowerShell (aborta por causa do stderr).
