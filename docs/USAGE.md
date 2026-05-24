# Uso

## Modo interativo

Executar sem parâmetros abre o assistente no terminal:

```bash
SystemMonitorLogger
```

O assistente pergunta:

- Duração do monitoramento
- Intervalo de coleta
- Verificação de saúde física do disco (SMART)
- Modo de tela (dashboard ou simples)

---

## Modo direto

Com parâmetros, o monitoramento inicia imediatamente sem perguntas:

```bash
SystemMonitorLogger --duration 30m
```

---

## Parâmetros disponíveis

### `--duration`

Define a duração do monitoramento.

```bash
SystemMonitorLogger --duration 30m
SystemMonitorLogger --duration 1h
SystemMonitorLogger --duration 1h30m
SystemMonitorLogger --duration notime
```

Formatos aceitos:

```
m = minutos   h = horas   notime = sem limite (até CTRL+C)
Exemplos: 5m | 30m | 1h | 1h30m
```

### `--interval`

Define o intervalo de coleta em segundos. Padrão: `1`.

```bash
SystemMonitorLogger --interval 5
```

### `--no-smart`

Desativa a verificação de saúde física do disco.

```bash
SystemMonitorLogger --no-smart
```

### `--simple`

Usa saída textual compacta em vez do dashboard dinâmico. Recomendado para terminais antigos, logs automatizados ou ambientes com suporte ANSI limitado.

```bash
SystemMonitorLogger --simple
```

Se o terminal não suportar cursor/ANSI dinâmico, o aplicativo troca automaticamente para modo simples.

---

## Arquivos gerados

Cada execução cria uma subpasta em `./logs`:

```
logs/
└── 2026-05-24_18-30-00-123_COMPUTADOR-01_PID1234/
    ├── report.txt
    ├── samples.csv
    ├── processes.csv
    └── smart.txt
```

A pasta inclui data, hora, milissegundos, nome do computador e PID para evitar colisão quando duas execuções iniciam no mesmo segundo.

---

## Campos de `samples.csv`

```csv
timestamp,cpu_percent,memory_used_percent,memory_used_mb,memory_total_mb,disk_usage_percent,disk_free_mb,disk_total_mb,disk_read_mb_s,disk_write_mb_s
```

`disk_read_mb_s` e `disk_write_mb_s` indicam leitura/escrita por segundo quando o sistema operacional disponibiliza esses contadores.

---

## Campos de `processes.csv`

```csv
timestamp,process_name,display_name,pid,cpu_percent,memory_mb
```

No dashboard e no relatório, processos com o mesmo nome amigável são agrupados. O CSV mantém cada PID individualmente para análise técnica.

---

## Permissões

O SMART pode exigir administrador (Windows) ou root (Linux). Quando não disponível, o aplicativo continua coletando CPU, RAM, disco e processos normalmente.
