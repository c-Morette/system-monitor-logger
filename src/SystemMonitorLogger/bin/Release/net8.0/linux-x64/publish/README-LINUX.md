# Como executar no Linux

Este pacote contem o executavel Linux x64 do SystemMonitorLogger.

## 1. Abrir o terminal na pasta do arquivo

Entre na pasta onde o arquivo `SystemMonitorLogger-linux-x64` foi baixado.

Exemplo:

```bash
cd ~/Downloads
```

## 2. Dar permissao de execucao

Em alguns casos o Linux baixa o arquivo sem permissao para executar. Rode:

```bash
chmod +x SystemMonitorLogger-linux-x64
```

## 3. Executar

Para iniciar o monitoramento:

```bash
./SystemMonitorLogger-linux-x64
```

Tambem pode executar direto com parametros:

```bash
./SystemMonitorLogger-linux-x64 --duration 30m
./SystemMonitorLogger-linux-x64 --duration 1h --interval 5
./SystemMonitorLogger-linux-x64 --duration 30m --simple
```

## SMART do disco

A leitura SMART pode exigir permissao de administrador/root.

Para tentar coletar SMART completo:

```bash
sudo ./SystemMonitorLogger-linux-x64
```

Se nao quiser coletar SMART:

```bash
./SystemMonitorLogger-linux-x64 --no-smart
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
