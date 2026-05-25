# Como executar no Linux

Este pacote contem o executavel Linux x64 do SystemMonitorLogger.

## 1. Abrir o terminal na pasta do arquivo

Entre na pasta onde o arquivo `SystemMonitorLogger` foi baixado.

Exemplo:

```bash
cd ~/Downloads
```

## 2. Dar permissao de execucao

Em alguns casos o Linux baixa o arquivo sem permissao para executar. Rode:

```bash
chmod +x SystemMonitorLogger
```

## 3. Executar

Para iniciar o monitoramento:

```bash
./SystemMonitorLogger
```

Tambem pode executar direto com parametros:

```bash
./SystemMonitorLogger --duration 30m
./SystemMonitorLogger --duration 1h --interval 5
./SystemMonitorLogger --duration 30m --simple
```

## SMART do disco

A leitura SMART pode exigir permissao de administrador/root.

Para tentar coletar SMART completo:

```bash
sudo ./SystemMonitorLogger
```

Se nao quiser coletar SMART:

```bash
./SystemMonitorLogger --no-smart
```

## Onde ficam os logs

Cada execucao cria uma pasta `logs` ao lado do executavel:

```text
logs/
  2026-05-24_18-30-00-123_COMPUTADOR_PID1234/
    report.txt
    samples.csv
    processes.csv
    smart.txt
```

O arquivo principal para leitura rapida e o `report.txt`.
