#pragma once

#include <string>
#include <vector>

namespace Console
{
    // Habilita o modo VT no Windows (10/11) para permitir redraw sem piscar.
    // No Win7 (sem VT) usa o fallback. Chamar uma vez antes do loop ao vivo.
    void Init();

    // Desenha um quadro sem piscar: leva o cursor ao topo e sobrescreve as
    // linhas (em vez de limpar a tela inteira a cada tick).
    void DrawFrame(const std::vector<std::string>& lines);

    // Barra de progresso textual, ex.: "[#####-----] 52.0%".
    std::string Bar(double percent, int width = 20);
}
