#pragma once

#include <string>

namespace FileHelper
{
    // Nome da maquina (hostname).
    std::string MachineName();

    // PID do processo atual.
    long ProcessId();

    // Descricao curta do SO, ex.: "Linux 32-bit", "Windows 64-bit".
    std::string OperatingSystem();

    // Cria (e retorna) uma pasta unica para a execucao em <exe>/logs/,
    // no formato "yyyy-MM-dd_HH-mm-ss-fff_<host>_PID<pid>".
    std::string CreateRunDirectory();
}
