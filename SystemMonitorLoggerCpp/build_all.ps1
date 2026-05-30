# Gera os 4 binarios a partir da maquina Windows 64-bit, TODOS via Docker.
# Nada precisa estar instalado na maquina alem do Docker.
#   linux-x64 / linux-x86 -> g++ nativo / -m32
#   win-x64   / win-x86   -> cross-compile com MinGW-w64
#
# O container e' isolado: nao toca em outros containers (ex.: Postgres).
#
# Uso:  .\build_all.ps1                      (os 4 alvos)
#       .\build_all.ps1 -Targets linux-x86   (so um ou alguns)
[CmdletBinding()]
param(
    [string[]]$Targets = @("linux-x64", "linux-x86", "win-x64", "win-x86")
)

$ErrorActionPreference = "Stop"
$root  = $PSScriptRoot
$dist  = Join-Path $root "dist"
$image = "smlogger-build"
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Write-Host "==> Build da imagem Docker..." -ForegroundColor Cyan
docker build -f "$root\docker\Dockerfile.linux" -t $image $root
if ($LASTEXITCODE -ne 0) { throw "docker build falhou" }

foreach ($t in $Targets) {
    Write-Host "==> Compilando $t ..." -ForegroundColor Cyan
    docker run --rm -v "${root}:/work" $image $t
    if ($LASTEXITCODE -ne 0) { throw "build $t falhou" }
}

Write-Host "`n==> Binarios em: $dist" -ForegroundColor Green
Get-ChildItem $dist | Select-Object Name, Length | Format-Table -AutoSize
