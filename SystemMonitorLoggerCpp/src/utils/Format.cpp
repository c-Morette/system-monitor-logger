#include "utils/Format.hpp"

#include <cstdio>

std::string FormatNum(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    std::string text(buffer);

    const auto dot = text.find('.');
    if (dot != std::string::npos)
    {
        std::size_t last = text.size() - 1;
        while (text[last] == '0')
        {
            --last;
        }
        if (text[last] == '.')
        {
            --last;
        }
        text.erase(last + 1);
    }

    return text;
}
