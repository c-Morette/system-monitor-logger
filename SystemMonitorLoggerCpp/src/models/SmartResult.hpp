#pragma once

#include <optional>
#include <string>

// Resultado da coleta SMART. Campos opcionais espelham os nullable do C#.
struct SmartResult
{
    bool available = false;
    std::string status;
    std::string details;
    std::string device;
    std::optional<int> temperatureCelsius;
    std::optional<long long> powerOnHours;
    std::optional<long long> reallocatedSectors;
    std::optional<long long> pendingSectors;
    std::optional<bool> smartPassed;

    static SmartResult Failed(const std::string& reason)
    {
        SmartResult r;
        r.available = false;
        r.status = "Nao disponivel ou incompleto";
        r.details = reason;
        return r;
    }
};
