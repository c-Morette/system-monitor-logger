# SystemMonitorLogger (C++) — Linux

Binário único, **sem dependências** (linkado 100% estático — roda em Lubuntu antigos sem
problema de versão de glibc).

## Qual binário usar

| Sistema | Arquivo |
|---------|---------|
| Linux 32-bit (i386 / Lubuntu antigo) | `SystemMonitorLogger-linux-x86` |
| Linux 64-bit | `SystemMonitorLogger-linux-x64` |

Na dúvida sobre 32 ou 64 bits:

```bash
uname -m       # i686/i386 = 32-bit ; x86_64 = 64-bit
```

## Como executar

```bash
chmod +x SystemMonitorLogger-linux-x86
./SystemMonitorLogger-linux-x86                 # roda até CTRL+C (tela ao vivo)
./SystemMonitorLogger-linux-x86 --duration 30m  # 30 minutos
./SystemMonitorLogger-linux-x86 --duration 1h --interval 5 --simple
```

Opções: `--duration` (30m, 1h, 1h30m, 90s, notime), `--interval <seg>`,
`--no-smart`, `--simple`, `--help`.

## SMART (saúde do disco)

O SMART usa o `smartctl` **do sistema** (não vem embutido). Para habilitar:

```bash
sudo apt install smartmontools
```

E execute como root para ler todos os dados do disco:

```bash
sudo ./SystemMonitorLogger-linux-x86
```

Sem `smartctl` ou sem root, o monitoramento continua normal — o SMART apenas fica
"incompleto" no relatório.

## Saídas

Cada execução cria uma pasta em `./logs` (ao lado do binário):

```
logs/2026-05-24_18-30-00-123_PC-01_PID1234/
├── report.txt      ← relatório com diagnóstico
├── samples.csv     ← CPU/RAM/disco por amostra
├── processes.csv   ← top processos
└── smart.txt       ← saúde do disco (se disponível)
```
