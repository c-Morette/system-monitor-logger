# SMART

O SystemMonitorLogger usa `smartctl` para coletar informações de saúde física do disco.

---

## Como funciona

1. Verifica se há um binário `smartctl` embutido no executável
2. Se existir, extrai para uma pasta temporária
3. Se não existir, tenta usar `smartctl` instalado no sistema via `PATH`
4. Executa `smartctl --scan-open` para descobrir o dispositivo
5. Executa `smartctl -H -A <dispositivo>`
6. Grava o resultado em `smart.txt`
7. Remove os arquivos temporários ao encerrar

Se o SMART falhar por qualquer motivo, o monitoramento continua normalmente — apenas o `smart.txt` ficará com o registro do erro.

---

## Campos interpretados

| Campo | Descrição |
|-------|-----------|
| Status geral | `PASSED` ou `FAILED` |
| Temperatura | Temperatura atual do disco em °C |
| Horas de uso | Total de horas ligado |
| Setores realocados | Setores com defeito realocados pelo disco |
| Setores pendentes | Setores aguardando realocação (sinal de alerta) |

---

## Permissões

O SMART geralmente exige privilégios elevados:

- **Windows:** execute como Administrador
- **Linux:** execute como root ou com `sudo`

Sem permissão, o aplicativo exibe um aviso e continua coletando CPU, RAM, disco e processos normalmente.

---

## Binários embutidos

| Plataforma | Versão | Caminho no projeto |
|------------|--------|--------------------|
| Windows x64 | smartmontools 7.5 | `Resources/smartctl/windows/smartctl.exe` |
| Linux amd64 | smartmontools 7.4 (Ubuntu Noble) | `Resources/smartctl/linux/smartctl` |

Os binários são incluídos como `EmbeddedResource` no `.csproj` e extraídos para:

- **Windows:** `%TEMP%\SystemMonitorLogger\smartctl.exe`
- **Linux:** `/tmp/SystemMonitorLogger/smartctl`

---

## Licença dos binários

Os binários do `smartctl` são do projeto **smartmontools**, licenciado sob **GNU GPL v2**.

Veja [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) para detalhes completos.
