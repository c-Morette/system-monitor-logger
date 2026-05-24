# SystemMonitorLogger

## 1. Visão geral

O **SystemMonitorLogger** é uma ferramenta de terminal desenvolvida em C#/.NET para monitorar o uso de recursos do computador e gerar logs simples para análise técnica.

A ideia principal é ajudar no diagnóstico de computadores que apresentam lentidão, principalmente em ambientes de trabalho, PDVs e máquinas antigas, onde nem sempre é fácil identificar se o problema está relacionado à CPU, memória RAM, disco, espaço livre ou saúde física do armazenamento.

O projeto deve ser simples, direto e adequado para um contexto acadêmico, sem arquitetura exagerada e sem dependências complexas.

---

## 2. Objetivo do projeto

Criar um aplicativo de console capaz de:

- Monitorar CPU, memória RAM e disco.
- Registrar amostras periódicas em arquivo CSV.
- Gerar um relatório final em TXT.
- Identificar possíveis gargalos com base em regras simples.
- Verificar informações SMART do disco quando possível.
- Funcionar em Windows 10, Windows 11 e Linux/Lubuntu.
- Ser distribuído como executável único, sem arquivos obrigatórios soltos.

O aplicativo não tem como objetivo substituir ferramentas profissionais de diagnóstico, mas sim oferecer uma solução simples, portátil e útil para suporte técnico básico.

---

## 3. Nome do projeto

Nome definido:

```txt
SystemMonitorLogger
```

Motivo do nome:

- É direto.
- Descreve bem a função da ferramenta.
- Não usa branding comercial.
- Combina com um projeto acadêmico/open source.

---

## 4. Escopo do projeto

### Dentro do escopo

- Aplicativo de terminal.
- Monitoramento local do computador.
- Coleta periódica de métricas.
- Geração de logs.
- Geração de relatório final.
- Diagnóstico simples baseado em limites.
- Compatibilidade com Windows 10, Windows 11 e Linux/Lubuntu.
- Execução como arquivo único.
- SMART usando smartctl embutido e extraído temporariamente.

### Fora do escopo inicial

- Interface gráfica.
- Dashboard web.
- Banco de dados.
- API.
- Serviço em segundo plano.
- Instalador complexo.
- Upload automático dos logs.
- Machine learning.
- Sistema de usuários.
- Windows 7.

O Windows 7 poderá ser tratado futuramente como uma versão legado separada.

---

## 5. Tecnologias sugeridas

```txt
Linguagem: C#
Plataforma: .NET 8 LTS
Tipo: Console App
Interface: Spectre.Console
Compatibilidade: Windows 10, Windows 11, Linux e Lubuntu
Distribuição: Single-file self-contained
```

### Justificativa do .NET 8

O .NET 8 é uma versão LTS, estável e adequada para projetos modernos. Ele permite criar aplicações console multiplataforma e publicar o aplicativo como executável único, sem exigir que o usuário tenha o .NET instalado.

---

## 6. Compatibilidade

Compatibilidade oficial da primeira versão:

```txt
Windows 10
Windows 11
Linux
Lubuntu
```

Fora do escopo oficial:

```txt
Windows 7
Windows 8
Windows 8.1
```

Caso seja necessário no futuro, poderá ser criada uma versão legado específica para Windows 7.

---

## 7. Modo de execução

O aplicativo será executado diretamente pelo terminal.

Exemplo básico:

```bash
SystemMonitorLogger
```

Decisão de implementação:

```txt
Quando nenhum parâmetro é informado, o aplicativo abre um assistente interativo com Spectre.Console para perguntar duração, intervalo, verificação da saúde física do disco e modo de tela.
Quando parâmetros são informados, o monitoramento inicia diretamente, sem perguntas.
Os logs sempre são gravados em ./logs, ao lado do executável/local de execução.
```

Comportamento padrão:

```txt
Intervalo de coleta: 1 segundo
Duração: até o usuário pressionar CTRL+C
SMART: ativado quando possível
Saída: pasta ./logs ao lado do executável
```

---

## 8. Parâmetros de linha de comando

O aplicativo deve aceitar alguns parâmetros simples.

### Definir duração

```bash
SystemMonitorLogger --duration 30m
```

Exemplos esperados:

```bash
SystemMonitorLogger --duration 5m
SystemMonitorLogger --duration 30m
SystemMonitorLogger --duration 1h
SystemMonitorLogger --duration 1h30m
SystemMonitorLogger --duration notime
```

Formato definido:

```txt
m = minutos
h = horas
notime
Exemplos: 30m | 1h | 1h30m
```

### Definir intervalo

```bash
SystemMonitorLogger --interval 5
```

O valor representa segundos.

Exemplos:

```bash
SystemMonitorLogger --interval 2
SystemMonitorLogger --interval 5
SystemMonitorLogger --interval 10
```

### Desativar SMART

```bash
SystemMonitorLogger --no-smart
```

### Modo simples

```bash
SystemMonitorLogger --simple
```

Esse modo deve usar uma saída mais básica no terminal, sem depender de uma tela dinâmica. Pode ser útil em terminais antigos ou limitados.

### Pasta de saída

A pasta de saída é fixa para manter o uso simples e previsível:

```txt
./logs
```

---

## 9. Métricas monitoradas

### CPU

Coletar:

- Uso atual da CPU em porcentagem.
- Média de uso durante o monitoramento.
- Pico máximo de uso.
- Quantidade de amostras acima do limite definido.

### Memória RAM

Coletar:

- Memória total.
- Memória usada.
- Memória livre.
- Uso da memória em porcentagem.
- Média de uso.
- Pico de uso.

### Disco

Coletar:

- Uso do disco em porcentagem.
- Espaço livre.
- Espaço total.
- Leitura por segundo, quando possível.
- Escrita por segundo, quando possível.
- Pico de uso.

Decisão de implementação:

```txt
Leitura e escrita por segundo são gravadas em MB/s nos campos disk_read_mb_s e disk_write_mb_s.
No Windows, é usada consulta nativa via DeviceIoControl quando disponível.
No Linux, os valores são calculados a partir de /proc/diskstats.
Caso a plataforma não disponibilize os dados, o aplicativo registra 0 e continua normalmente.
```

### Processos

Coletar periodicamente:

- Top processos por uso de memória.
- Top processos por uso de CPU, quando possível.

Decisão de implementação:

```txt
A tela separa Top CPU e Top RAM para evitar que processos com uso mínimo de CPU apareçam como mais importantes que processos com alto consumo de memória.
No dashboard e no relatório, processos com o mesmo nome são agrupados para aproximar a leitura do Gerenciador de Tarefas.
Quando possível, o dashboard e o relatório usam o nome amigável do aplicativo lendo metadados do executável, como Visual Studio Code em vez de Code.
No Linux, o aplicativo tenta melhorar o nome usando /proc/<pid>/cmdline quando os metadados não estiverem disponíveis.
O arquivo processes.csv mantém cada PID individual para análise detalhada, registrando process_name e display_name.
```

Sugestão:

```txt
Amostras do sistema: a cada 1 segundo
Amostras de processos: a cada 5 segundos
```

### SMART

Quando disponível, coletar:

- Status geral do SMART.
- Temperatura do disco.
- Horas de uso.
- Setores realocados.
- Setores pendentes.
- Erros importantes informados pelo smartctl.

---

## 10. SMART e smartctl

Para consultar a saúde física do disco, o projeto poderá usar o `smartctl`.

Como o objetivo é não deixar arquivos soltos junto do programa, o `smartctl` deverá ser incluído como recurso interno do executável.

Decisão de implementação:

```txt
O aplicativo primeiro tenta usar smartctl embutido como EmbeddedResource.
Caso o binário ainda não esteja presente no projeto, tenta usar smartctl instalado no sistema via PATH.
Essa abordagem permite desenvolvimento e testes antes da inclusão final dos binários.
```

Fluxo esperado:

1. O aplicativo inicia.
2. Verifica se a coleta SMART está ativada.
3. Extrai temporariamente o smartctl para uma pasta temporária.
4. Executa o smartctl.
5. Salva o resultado em `smart.txt`.
6. Remove o arquivo temporário ao finalizar, quando possível.

Exemplo de pasta temporária no Windows:

```txt
%TEMP%/SystemMonitorLogger/smartctl.exe
```

Exemplo de pasta temporária no Linux:

```txt
/tmp/SystemMonitorLogger/smartctl
```

Caso o SMART falhe, o aplicativo não deve encerrar. Ele deve apenas registrar o erro no relatório.

Exemplo:

```txt
SMART não disponível ou incompleto.
Motivo: o aplicativo não está sendo executado como administrador/root, o disco não é compatível ou houve erro ao executar smartctl.
```

---

## 11. Permissões

O aplicativo pode ser executado com privilégios administrativos/root, principalmente para permitir a leitura SMART.

Porém, caso não esteja rodando com permissões elevadas, ele não deve encerrar automaticamente.

Comportamento esperado:

```txt
Avisar o usuário.
Continuar monitorando CPU, RAM e disco.
Registrar no relatório que o SMART pode estar incompleto.
```

Exemplo de aviso:

```txt
Aviso: o aplicativo não está rodando como administrador/root.
Algumas informações, principalmente SMART, podem não estar disponíveis.
```

---

## 12. Saída dos arquivos

Os logs devem ser criados em uma pasta `logs` ao lado do executável.

Exemplo:

```txt
SystemMonitorLogger.exe
logs/
```

Cada execução deve criar uma nova pasta com data, hora e nome do computador.

Decisão de implementação:

```txt
A pasta também inclui milissegundos e PID do processo para evitar colisão quando duas execuções iniciam no mesmo segundo.
```

Exemplo:

```txt
logs/
└── 2026-05-24_18-30-00_PDV-CAIXA-03/
    ├── report.txt
    ├── samples.csv
    ├── processes.csv
    └── smart.txt
```

No final da execução, o aplicativo deve apenas mostrar o caminho do relatório.

Exemplo:

```txt
Monitoramento finalizado.
Relatório gerado em:
logs/2026-05-24_18-30-00_PDV-CAIXA-03/report.txt
```

O relatório não deve abrir automaticamente.

---

## 13. Arquivos gerados

### report.txt

Arquivo principal para leitura humana.

Deve conter:

- Informações da máquina.
- Sistema operacional.
- Data e hora de início.
- Data e hora de fim.
- Duração total.
- Resumo das métricas.
- Diagnóstico provável.
- Recomendações simples.
- Status SMART, quando disponível.

### samples.csv

Arquivo com as amostras do sistema.

Campos sugeridos:

```csv
timestamp,cpu_percent,memory_used_percent,memory_used_mb,memory_total_mb,disk_usage_percent,disk_read_mb_s,disk_write_mb_s
```

### processes.csv

Arquivo com amostras dos principais processos.

Campos sugeridos:

```csv
timestamp,process_name,pid,cpu_percent,memory_mb
```

Implementação atual:

```csv
timestamp,process_name,display_name,pid,cpu_percent,memory_mb
```

### smart.txt

Arquivo com a saída resumida ou completa da leitura SMART.

Caso o SMART não esteja disponível, registrar o motivo.

---

## 14. Exemplo de saída no terminal

Exemplo com interface simples usando Spectre.Console:

```txt
SystemMonitorLogger

Monitorando computador local...

Tempo: 00:08:35
Amostras: 103
Intervalo: 5s

CPU     Atual: 34%   Média: 41%   Pico: 89%
RAM     Atual: 78%   Média: 74%   Pico: 91%
DISCO   Atual: 100%  Média: 52%   Pico: 100%

Top processos:
1. PDV.exe        RAM: 820 MB
2. chrome.exe     RAM: 610 MB
3. MsMpEng.exe    RAM: 340 MB

Pressione CTRL+C para finalizar e gerar o relatório.
```

A interface deve ser limpa e objetiva. O foco é utilidade, não aparência exagerada.

Decisão de implementação:

```txt
O modo padrão usa Spectre.Console LiveDisplay para atualizar a tela sem limpar o terminal inteiro.
O parâmetro --simple mantém uma saída textual compacta para terminais antigos ou limitados.
```

---

## 15. Relatório final esperado

Exemplo de relatório:

```txt
SystemMonitorLogger - Relatório de Diagnóstico

Computador: PDV-CAIXA-03
Sistema: Windows 10
Início: 24/05/2026 18:30:00
Fim: 24/05/2026 19:00:00
Duração: 30 minutos
Intervalo: 5 segundos

Resumo:
CPU média: 42%
CPU pico: 91%

RAM média: 78%
RAM pico: 92%

Disco média: 61%
Disco pico: 100%

SMART:
Status: Atenção
Setores pendentes: 2
Temperatura: 42°C

Diagnóstico provável:
O principal gargalo identificado foi o DISCO.

Durante o monitoramento, o disco atingiu 100% de uso diversas vezes.
Esse comportamento pode causar lentidão, travamentos temporários e demora na abertura de programas.

Recomendações:
1. Verificar se o computador utiliza HD mecânico.
2. Verificar a saúde SMART do disco.
3. Considerar troca para SSD.
4. Verificar se a memória RAM está causando paginação em disco.
```

---

## 16. Regras simples de diagnóstico

O diagnóstico deve ser baseado em regras simples, fáceis de explicar.

### CPU

```txt
Alerta médio: CPU acima de 80% por várias amostras.
Alerta alto: CPU acima de 90% com frequência.
Possível gargalo: média acima de 80% ou muitos picos acima de 90%.
```

### Memória RAM

```txt
Alerta médio: RAM acima de 85%.
Alerta alto: RAM acima de 90%.
Possível gargalo: memória alta constante ou pouca memória livre.
```

### Disco

```txt
Alerta médio: disco acima de 80%.
Alerta alto: disco acima de 95%.
Possível gargalo: disco em 100% com frequência ou uso alto constante.
```

### Espaço livre

```txt
Alerta: espaço livre abaixo de 15%.
Crítico: espaço livre abaixo de 5%.
```

### SMART

```txt
Crítico: SMART failed.
Alerta: setores pendentes acima de 0.
Alerta: setores realocados elevados.
Alerta: temperatura alta.
```

---

## 17. Diagnóstico provável

O relatório pode classificar o gargalo principal como:

```txt
CPU
MEMÓRIA
DISCO
SMART
INCONCLUSIVO
```

Exemplo:

```txt
Possível gargalo principal: DISCO
```

Caso mais de um problema seja encontrado, o relatório pode listar prioridades.

Exemplo:

```txt
Possíveis gargalos encontrados:
1. DISCO - alto uso constante e picos de 100%.
2. MEMÓRIA - uso acima de 90% em várias amostras.
```

Caso nada relevante seja encontrado:

```txt
Diagnóstico inconclusivo.
Durante o período monitorado, não foram encontrados picos significativos de CPU, memória ou disco.
Recomenda-se repetir o teste durante o momento em que a lentidão ocorrer.
```

---

## 18. Estrutura de pastas do projeto

Estrutura sugerida:

```txt
SystemMonitorLogger/
│
├── src/
│   └── SystemMonitorLogger/
│       │
│       ├── Program.cs
│       ├── AppSettings.cs
│       │
│       ├── Models/
│       │   ├── SystemSample.cs
│       │   ├── ProcessSample.cs
│       │   ├── DiskInfo.cs
│       │   ├── SmartInfo.cs
│       │   └── MonitorSummary.cs
│       │
│       ├── Services/
│       │   ├── MonitorService.cs
│       │   ├── ReportService.cs
│       │   ├── CsvLogService.cs
│       │   ├── ProcessService.cs
│       │   └── SmartService.cs
│       │
│       ├── Platform/
│       │   ├── ISystemMetricsProvider.cs
│       │   ├── WindowsMetricsProvider.cs
│       │   └── LinuxMetricsProvider.cs
│       │
│       ├── Utils/
│       │   ├── AdminHelper.cs
│       │   ├── TimeHelper.cs
│       │   └── FileHelper.cs
│       │
│       └── Resources/
│           └── smartctl/
│               ├── windows/
│               │   └── smartctl.exe
│               └── linux/
│                   └── smartctl
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── USAGE.md
│   └── EXAMPLES.md
│
├── logs/
│   └── .gitkeep
│
├── README.md
├── .gitignore
└── SystemMonitorLogger.sln
```

---

## 19. Explicação das principais classes

### Program.cs

Responsável por iniciar o aplicativo.

Funções principais:

- Ler argumentos.
- Verificar o sistema operacional.
- Verificar permissões.
- Criar a pasta de logs.
- Iniciar o monitoramento.
- Capturar CTRL+C.
- Gerar relatório final.

### AppSettings.cs

Representa as configurações básicas do programa.

Exemplo de propriedades:

```txt
IntervalSeconds
Duration
EnableSmart
SimpleMode
TopProcessCount
```

### MonitorService.cs

Serviço principal do monitoramento.

Responsabilidades:

- Executar o loop de coleta.
- Pedir amostras ao provider da plataforma.
- Atualizar a tela.
- Enviar dados para o CSV.
- Guardar dados para o relatório final.

### ReportService.cs

Responsável por gerar o `report.txt`.

Responsabilidades:

- Calcular médias.
- Calcular picos.
- Identificar alertas.
- Definir diagnóstico provável.
- Escrever recomendações.

### CsvLogService.cs

Responsável por gravar arquivos CSV.

Arquivos:

```txt
samples.csv
processes.csv
```

### ProcessService.cs

Responsável por consultar os processos em execução.

Responsabilidades:

- Listar processos.
- Ordenar por memória.
- Ordenar por CPU, quando possível.
- Gerar amostras para `processes.csv`.

### SmartService.cs

Responsável por consultar o SMART.

Responsabilidades:

- Extrair smartctl temporariamente.
- Executar smartctl.
- Coletar informações básicas.
- Salvar `smart.txt`.
- Apagar arquivos temporários.

### ISystemMetricsProvider.cs

Interface comum para coleta de métricas.

Exemplo:

```csharp
public interface ISystemMetricsProvider
{
    Task<SystemSample> GetSystemSampleAsync();
    Task<List<DiskInfo>> GetDisksAsync();
}
```

### WindowsMetricsProvider.cs

Implementação para Windows.

Responsável por coletar métricas em Windows 10 e Windows 11.

### LinuxMetricsProvider.cs

Implementação para Linux/Lubuntu.

Responsável por coletar métricas usando recursos do Linux, como `/proc` e `/sys`, quando necessário.

---

## 20. Publicação do aplicativo

A publicação deve gerar um executável único por sistema operacional.

### Windows x64

```bash
dotnet publish -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```

Resultado esperado:

```txt
SystemMonitorLogger.exe
```

### Linux x64

```bash
dotnet publish -c Release -r linux-x64 --self-contained true /p:PublishSingleFile=true
```

Resultado esperado:

```txt
SystemMonitorLogger
```

Observação:

Mesmo usando o mesmo código-fonte, é necessário gerar um executável para cada sistema operacional.

---

## 21. Cuidados com privacidade

O aplicativo deve evitar coletar informações sensíveis.

Não coletar:

- Conteúdo de arquivos.
- Histórico de navegação.
- Janelas abertas.
- Caminhos completos desnecessários.
- Dados pessoais de usuários.

Coletar apenas:

- Nome do processo.
- PID, se necessário.
- Uso de CPU.
- Uso de memória.
- Métricas gerais do sistema.
- Informações técnicas do disco.

Isso torna o projeto mais adequado para uso em ambiente de trabalho.

---

## 22. MVP sugerido

A primeira versão deve ser simples.

### MVP 1

- Criar Console App em C#/.NET.
- Detectar Windows ou Linux.
- Monitorar CPU.
- Monitorar memória RAM.
- Monitorar disco.
- Criar pasta de logs.
- Salvar `samples.csv`.
- Gerar `report.txt`.
- Parar com CTRL+C.
- Aceitar `--duration`.
- Aceitar `--interval`.

### MVP 2

- Adicionar top processos.
- Gerar `processes.csv`.
- Melhorar relatório final.
- Adicionar diagnóstico provável.

Status de implementação:

```txt
Implementado.
Processos são amostrados a cada 5 segundos.
CPU por processo é calculada entre amostras.
O relatório inclui processos com maior uso observado e evidências do diagnóstico.
```

### MVP 3

- Adicionar SMART.
- Embutir smartctl.
- Extrair smartctl temporariamente.
- Gerar `smart.txt`.
- Adicionar regras de alerta SMART.

Status de implementação:

```txt
Parcialmente implementado.
O serviço SMART já descobre dispositivo, executa smartctl, gera smart.txt e interpreta status, temperatura, horas de uso, setores realocados e setores pendentes.
O projeto já está preparado para embutir smartctl em Resources/smartctl/windows e Resources/smartctl/linux.
Os binários reais ainda precisam ser adicionados ao projeto para fechar a distribuição totalmente offline.
```

Atualização:

```txt
Binários do smartctl adicionados ao projeto.
Windows: smartmontools 7.5.
Linux amd64: pacote Ubuntu Noble smartmontools 7.4.
O projeto inclui aviso de terceiros em THIRD_PARTY_NOTICES.md.
```

---

## 23. Filosofia do projeto

O projeto deve ser:

```txt
Simples
Portável
Objetivo
Fácil de explicar
Fácil de testar
Útil em ambiente real
Adequado para apresentação acadêmica
```

O projeto não deve tentar ser uma solução corporativa completa. O foco é resolver um problema real de forma simples: registrar o comportamento do computador durante um período e gerar um relatório que ajude no diagnóstico de lentidão.

---

## 24. Resumo final das decisões

```txt
Nome: SystemMonitorLogger
Tipo: Console App
Linguagem: C#
Plataforma: .NET 8 LTS
Interface: Spectre.Console
Compatibilidade: Windows 10, Windows 11, Linux e Lubuntu
Windows 7: fora do escopo atual
Distribuição: single-file self-contained
Logs: pasta ./logs ao lado do executável
Relatório: report.txt
Amostras: samples.csv
Processos: processes.csv
SMART: smart.txt
SMART externo: smartctl embutido e extraído temporariamente
Permissões: avisar se não for admin/root, mas continuar
Modo padrão: rodar até CTRL+C
Intervalo padrão: 1 segundo
Arquitetura: simples, organizada e sem overengineering
```

---

## 25. Frase curta para apresentação

O **SystemMonitorLogger** é uma ferramenta de terminal para monitoramento local de CPU, memória, disco e saúde SMART, gerando logs e relatórios simples para auxiliar no diagnóstico de lentidão em computadores de trabalho.
