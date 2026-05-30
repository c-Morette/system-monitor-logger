#pragma once

#include <optional>
#include <string>

#include "AppSettings.hpp"

namespace Args
{
    // Converte "30m", "1h", "1h30m", "90s", "notime" em segundos.
    // Retorna -1 para "notime" (sem limite) e nullopt se invalido.
    std::optional<long long> ParseDuration(const std::string& text);

    // Interpreta os argumentos. Em caso de erro, preenche 'error' e retorna false.
    bool Parse(int argc, char** argv, AppSettings& settings, std::string& error);

    // Texto de ajuda (--help).
    std::string HelpText();
}
