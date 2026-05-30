#pragma once

// Configuracoes de uma execucao (vindas dos argumentos de linha de comando).
struct AppSettings
{
    // Duracao total em segundos. -1 = sem limite (roda ate CTRL+C).
    long long durationSeconds = -1;
    int intervalSeconds = 1;
    bool smartEnabled = true;
    bool simpleMode = false;
};
