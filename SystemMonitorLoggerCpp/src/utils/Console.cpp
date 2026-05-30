#include "utils/Console.hpp"
#include "utils/Format.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace
{
    // Verdadeiro quando podemos usar codigos VT (ANSI): Linux sempre; Windows
    // apenas se o console aceitar o modo VT (Windows 10/11). No Win7 fica falso.
    bool g_vtEnabled = false;

#if defined(_WIN32)
    // Fallback para consoles sem VT (Win7): limpa a tela via API. Pisca, mas so
    // e usado quando o VT nao esta disponivel.
    void Win32ClearAndPrint(const std::vector<std::string>& lines)
    {
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (out == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(out, &info))
        {
            // Saida redirecionada (sem console): imprime simples.
            for (const auto& line : lines)
            {
                std::fputs(line.c_str(), stdout);
                std::fputc('\n', stdout);
            }
            return;
        }

        const DWORD cells = static_cast<DWORD>(info.dwSize.X) * info.dwSize.Y;
        const COORD home = {0, 0};
        DWORD written = 0;
        FillConsoleOutputCharacterW(out, L' ', cells, home, &written);
        SetConsoleCursorPosition(out, home);
        for (const auto& line : lines)
        {
            std::fputs(line.c_str(), stdout);
            std::fputc('\n', stdout);
        }
        std::fflush(stdout);
    }
#endif
}

namespace Console
{
    void Init()
    {
#if defined(_WIN32)
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        {
            if (SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                g_vtEnabled = true;
            }
        }
#else
        g_vtEnabled = true;
#endif
    }

    void DrawFrame(const std::vector<std::string>& lines)
    {
        if (g_vtEnabled)
        {
            // Sem piscar: cursor para o topo, cada linha apaga ate o fim
            // (\x1b[K) e ao final limpa o que sobrou abaixo (\x1b[J).
            std::string frame = "\x1b[H";
            for (const auto& line : lines)
            {
                frame += line;
                frame += "\x1b[K\n";
            }
            frame += "\x1b[J";
            std::fputs(frame.c_str(), stdout);
            std::fflush(stdout);
            return;
        }

#if defined(_WIN32)
        Win32ClearAndPrint(lines);
#else
        for (const auto& line : lines)
        {
            std::fputs(line.c_str(), stdout);
            std::fputc('\n', stdout);
        }
        std::fflush(stdout);
#endif
    }

    std::string Bar(double percent, int width)
    {
        const double clamped = std::clamp(percent, 0.0, 100.0);
        const int filled = static_cast<int>(clamped / 100.0 * width + 0.5);

        std::string bar = "[";
        for (int i = 0; i < width; ++i)
        {
            bar += i < filled ? '#' : '-';
        }
        bar += "] " + FormatNum(clamped) + "%";
        return bar;
    }
}
