#pragma once

#include <string>

#include "models/SmartResult.hpp"

// Coleta SMART chamando o `smartctl` do sistema (PATH). Se nao estiver
// instalado/acessivel, retorna um resultado "incompleto" sem interromper o
// monitoramento. Sempre grava smart.txt na pasta da execucao.
class SmartService
{
public:
    explicit SmartService(const std::string& runDirectory);

    SmartResult Collect();

private:
    std::string m_runDirectory;

    void WriteSmartFile(const SmartResult& result);
};
