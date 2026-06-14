#pragma once

#include "models/IncidentConfig.hpp"

// Configuracoes de uma execucao (vindas dos argumentos de linha de comando).
struct AppSettings
{
    // Duracao total em segundos. -1 = sem limite (roda ate CTRL+C).
    long long durationSeconds = -1;
    int intervalSeconds = 1;
    bool smartEnabled = true;
    bool simpleMode = false;
    // Modo silencioso: nao desenha NADA na tela (so grava CSV/relatorio).
    // Ideal para deixar rodando num PDV em uso, sem o operador ver nada.
    bool quietMode = false;
    // Sensibilidade da deteccao de incidentes (ver --sensitivity).
    IncidentConfig incident;
    // Regrava o report.txt a cada N segundos durante a execucao, para sobreviver
    // a quedas de energia / travamentos em testes longos sem ninguem por perto.
    // 0 = desliga (so o relatorio final). Padrao: 300s (5 min).
    long long partialReportSeconds = 300;
};
