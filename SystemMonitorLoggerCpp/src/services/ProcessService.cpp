#include "services/ProcessService.hpp"

#include <algorithm>
#include <unordered_set>

std::vector<ProcessSample> ProcessService::SelectTop(const std::vector<ProcessSample>& all, int count)
{
    if (count <= 0 || all.empty())
    {
        return {};
    }

    std::vector<ProcessSample> byMemory = all;
    std::sort(byMemory.begin(), byMemory.end(),
              [](const ProcessSample& a, const ProcessSample& b) {
                  return a.memoryMb > b.memoryMb;
              });

    std::vector<ProcessSample> byCpu = all;
    std::sort(byCpu.begin(), byCpu.end(),
              [](const ProcessSample& a, const ProcessSample& b) {
                  if (a.cpuPercent != b.cpuPercent)
                  {
                      return a.cpuPercent > b.cpuPercent;
                  }
                  return a.memoryMb > b.memoryMb;
              });

    // Concatena top-memoria e top-cpu, remove PIDs repetidos preservando ordem,
    // e limita a count*2 (igual ao C#).
    std::vector<ProcessSample> result;
    std::unordered_set<std::int32_t> seen;
    const std::size_t take = static_cast<std::size_t>(count);

    auto addFrom = [&](const std::vector<ProcessSample>& source) {
        std::size_t added = 0;
        for (const auto& p : source)
        {
            if (added >= take)
            {
                break;
            }
            ++added;
            if (seen.insert(p.pid).second)
            {
                result.push_back(p);
            }
        }
    };

    addFrom(byMemory);
    addFrom(byCpu);
    return result;
}
