#pragma once

#include "AppSettings.hpp"

// Assistente interativo (usado quando o programa roda sem argumentos):
// pergunta duracao, intervalo, SMART e modo, mostra um resumo e confirma.
namespace InteractiveSettings
{
    // Preenche 'settings' a partir das respostas. Retorna false se o usuario
    // cancelar no final.
    bool Ask(AppSettings& settings);
}
