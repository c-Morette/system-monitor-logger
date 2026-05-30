#pragma once

#include <string>

// Formata um numero com ate 2 casas decimais, sem zeros a direita
// (equivale ao formato "0.##" do C#). Ex.: 5.0 -> "5", 5.40 -> "5.4".
std::string FormatNum(double value);
