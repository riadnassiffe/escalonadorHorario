<#
.SYNOPSIS
    Script de compilação do Escalonador de Horários para Windows (Conan 2 + CMake + MSVC + Qt 6)
.DESCRIPTION
    Instala as dependências via Conan 2, configura o CMake com o toolchain gerado,
    compila o executável e opcionalmente empacota as DLLs do Qt via windeployqt.
.EXAMPLE
    .\scripts\build_windows.ps1 -BuildType Release -QtDir "C:\Qt\6.8.0\msvc2022_64"
#>

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$BuildType = "Release",
    [string]$QtDir = $env:QTDIR,
    [switch]$DeployQt = $true
)

$ErrorActionPreference = "Stop"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Escalonador de Horários - Build Windows (Conan 2)" -ForegroundColor Cyan
Write-Host " Configuração: $BuildType" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Verificar ferramentas necessárias
function Check-Command($cmd, $name) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Error "Comando '$name' ($cmd) não encontrado no PATH. Instale-o ou execute no Developer PowerShell do Visual Studio."
    }
}

Check-Command "conan" "Conan Package Manager"
Check-Command "cmake" "CMake"

# 2. Localizar Qt6 caso não tenha sido passado como argumento
if (-not $QtDir) {
    if ($env:Qt6_DIR) {
        $QtDir = (Resolve-Path "$env:Qt6_DIR\..\..\..").Path
    } elseif (Test-Path "C:\Qt") {
        $found = Get-ChildItem -Path "C:\Qt" -Recurse -Depth 3 -Filter "msvc2022_64" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            $QtDir = $found.FullName
            Write-Host "Qt 6 detectado automaticamente em: $QtDir" -ForegroundColor Green
        }
    }
}

# 3. Conan Install
Write-Host "`n[1/3] Instalando dependências com Conan 2..." -ForegroundColor Yellow
$profileFile = "profiles/windows-msvc"
if ($BuildType -eq "Debug") {
    $profileFile = "profiles/windows-msvc-debug"
}

if (Test-Path $profileFile) {
    conan install . --profile:host $profileFile --profile:build default --build=missing
} else {
    conan install . --build=missing -s compiler.cppstd=20 -s build_type=$BuildType
}

# 4. Configuração e Build CMake
Write-Host "`n[2/3] Configurando e compilando com CMake..." -ForegroundColor Yellow

$cmakeArgs = @(
    "-B", "build",
    "-S", "."
)

# Caminho do toolchain gerado pelo Conan (no Windows com MSVC multi-config ou single-config)
if (Test-Path "build/generators/conan_toolchain.cmake") {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake"
} elseif (Test-Path "build/$BuildType/generators/conan_toolchain.cmake") {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=build/$BuildType/generators/conan_toolchain.cmake"
}

if ($QtDir) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir"
}

cmake @cmakeArgs
cmake --build build --config $BuildType --parallel

# 5. Localizar executável gerado
Write-Host "`n[3/3] Verificando binário gerado..." -ForegroundColor Yellow
$exePath = $null
$possiblePaths = @(
    "build\$BuildType\escalonadorHorarios.exe",
    "build\escalonadorHorarios.exe",
    "build\Release\escalonadorHorarios.exe"
)

foreach ($p in $possiblePaths) {
    if (Test-Path $p) {
        $exePath = $p
        break
    }
}

if ($exePath) {
    Write-Host "Build concluído com sucesso: $exePath" -ForegroundColor Green

    # Executar windeployqt para copiar DLLs do Qt caso solicitado
    if ($DeployQt -and $QtDir) {
        $windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
        if (Test-Path $windeployqt) {
            Write-Host "Empacotando dependências Qt com windeployqt..." -ForegroundColor Cyan
            & $windeployqt $exePath
            Write-Host "Deploy de dependências concluído na pasta do executável!" -ForegroundColor Green
        }
    }
} else {
    Write-Warning "Executável não localizado nos caminhos usuais. Verifique a pasta build/."
}
