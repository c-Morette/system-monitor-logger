cmake_minimum_required(VERSION 3.16)

project(SystemMonitorLogger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Por padrao gera binario de Release otimizado quando nada for especificado.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(SystemMonitorLogger
    src/main.cpp
    src/utils/TimeHelper.cpp
    src/utils/Format.cpp
    src/utils/FileHelper.cpp
    src/utils/Args.cpp
    src/utils/Console.cpp
    src/services/MonitorService.cpp
    src/services/InteractiveSettings.cpp
    src/services/CsvLogService.cpp
    src/services/ReportService.cpp
    src/services/ProcessService.cpp
    src/services/SmartService.cpp
    src/platform/LinuxMetricsProvider.cpp
    src/platform/WindowsMetricsProvider.cpp
    src/platform/LinuxProcessProvider.cpp
    src/platform/WindowsProcessProvider.cpp
)

target_include_directories(SystemMonitorLogger PRIVATE src)

# Large File Support: no Linux 32-bit, garante que statvfs use contadores de
# 64 bits (statvfs64) para nao estourar em discos grandes. No-op no 64-bit.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(SystemMonitorLogger PRIVATE _FILE_OFFSET_BITS=64)
endif()

# Bibliotecas do Windows: psapi (GetProcessMemoryInfo), version (FileVersionInfo),
# pdh (contadores de desempenho de disco - Performance Data Helper).
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(SystemMonitorLogger PRIVATE psapi version pdh)
endif()

# Linkagem 100% estatica para o binario ser autossuficiente:
#  - Linux: inclui a propria glibc, evitando erro "GLIBC_x.xx not found" em
#    Lubuntu antigos (so usamos syscalls/procfs/popen, que funcionam estaticos).
#  - Windows (MinGW): inclui libstdc++/libgcc/winpthread, sem DLLs ao lado do .exe.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(SystemMonitorLogger PRIVATE
        -static
        -static-libstdc++
        -static-libgcc
    )
endif()
