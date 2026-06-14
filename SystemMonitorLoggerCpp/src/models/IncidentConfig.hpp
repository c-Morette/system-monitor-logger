#pragma once

// Limiares para a deteccao de incidentes no relatorio. Ajustaveis por preset
// (--sensitivity baixa|normal|alta) para o usuario calibrar sem recompilar.
struct IncidentConfig
{
    // Tempo minimo acima do limite para um pico virar "incidente".
    long long minSeconds = 30;
    // Limiares por recurso (em %).
    double cpu = 85.0;
    double ram = 90.0;
    double diskActive = 80.0;
};
