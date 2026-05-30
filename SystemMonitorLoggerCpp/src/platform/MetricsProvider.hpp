#pragma once

#include "models/SystemSample.hpp"

// Classe base abstrata (equivale a ISystemMetricsProvider do projeto C#).
// Em Enforce Script seria a classe que LinuxMetricsProvider/WindowsMetricsProvider
// "extends" e cujos metodos sao "override".
class MetricsProvider
{
public:
    virtual ~MetricsProvider() = default;

    // Metodo abstrato: cada plataforma coleta CPU/RAM/disco/IO do seu jeito.
    virtual SystemSample GetSystemSample() = 0;
};
