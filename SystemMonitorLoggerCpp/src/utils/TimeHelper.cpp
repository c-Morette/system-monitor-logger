#include "utils/TimeHelper.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
    std::tm LocalTime(std::time_t time)
    {
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        return tm;
    }
}

namespace TimeHelper
{
    std::string NowString()
    {
        const std::tm tm = LocalTime(std::time(nullptr));
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        return std::string(buffer);
    }

    std::string FolderTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()) % 1000;

        const std::tm tm = LocalTime(seconds);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);

        char result[40];
        std::snprintf(result, sizeof(result), "%s-%03d",
                      buffer, static_cast<int>(millis.count()));
        return std::string(result);
    }

    std::string FormatDateTime(std::time_t time)
    {
        const std::tm tm = LocalTime(time);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &tm);
        return std::string(buffer);
    }

    std::string FormatDuration(long long totalSeconds)
    {
        if (totalSeconds < 0)
        {
            totalSeconds = 0;
        }

        const long long hours = totalSeconds / 3600;
        const long long minutes = (totalSeconds % 3600) / 60;
        const long long seconds = totalSeconds % 60;

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld",
                      hours, minutes, seconds);
        return std::string(buffer);
    }

    void SleepMs(int milliseconds)
    {
        if (milliseconds <= 0)
        {
            return;
        }

#if defined(_WIN32)
        Sleep(static_cast<DWORD>(milliseconds));
#else
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000L;
        nanosleep(&ts, nullptr);
#endif
    }
}
