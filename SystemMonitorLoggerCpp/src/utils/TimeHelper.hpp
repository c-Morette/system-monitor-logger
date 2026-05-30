#pragma once

#include <ctime>
#include <string>

namespace TimeHelper
{
    // Data/hora local no formato "yyyy-MM-dd HH:mm:ss".
    std::string NowString();

    // Carimbo para nome de pasta: "yyyy-MM-dd_HH-mm-ss-fff" (com milissegundos).
    std::string FolderTimestamp();

    // Formata um time_t como "dd/MM/yyyy HH:mm:ss".
    std::string FormatDateTime(std::time_t time);

    // Formata uma duracao em segundos como "hh:mm:ss".
    std::string FormatDuration(long long totalSeconds);

    // Pausa a execucao. Portatil (evita std::this_thread, ausente no MinGW
    // com modelo de threads win32).
    void SleepMs(int milliseconds);
}
