# Exemplos

## Assistente interativo

```bash
SystemMonitorLogger
```

Abre o assistente com perguntas sobre duração, intervalo, SMART e modo de tela.

---

## Monitoramentos por duração

Monitorar por 5 minutos:

```bash
SystemMonitorLogger --duration 5m
```

Monitorar por 1 hora:

```bash
SystemMonitorLogger --duration 1h
```

Monitorar por 1 hora e 30 minutos:

```bash
SystemMonitorLogger --duration 1h30m
```

Monitorar sem limite (até CTRL+C):

```bash
SystemMonitorLogger --duration notime
```

---

## Ajustando o intervalo de coleta

Coletar a cada 5 segundos:

```bash
SystemMonitorLogger --duration 30m --interval 5
```

Coletar a cada 10 segundos:

```bash
SystemMonitorLogger --duration 1h --interval 10
```

---

## Desativando SMART

```bash
SystemMonitorLogger --duration 5m --no-smart
```

Útil quando não há permissão de administrador ou o disco não é compatível com SMART.

---

## Modo simples

```bash
SystemMonitorLogger --duration 30m --simple
```

Saída textual sem tela dinâmica — recomendado para terminais antigos ou sessões SSH.

---

## Observando I/O de disco

Para monitorar durante uma cópia de arquivos ou operação de disco intensa:

```bash
SystemMonitorLogger --duration 15m --interval 1
```

Os campos `disk_read_mb_s` e `disk_write_mb_s` em `samples.csv` mostram a atividade em MB/s.

---

## Via `dotnet run` (desenvolvimento)

```bash
dotnet run --project src/SystemMonitorLogger -- --duration 5m
dotnet run --project src/SystemMonitorLogger -- --duration 1m --interval 5 --no-smart
```
