@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo  Escalonador de Horarios - Build Windows (Conan 2)
echo ==================================================

set BUILD_TYPE=Release
if not "%~1"=="" set BUILD_TYPE=%~1

set PROFILE=profiles/windows-msvc
if /i "%BUILD_TYPE%"=="Debug" set PROFILE=profiles/windows-msvc-debug

echo [1/3] Executando Conan Install...
conan install . --profile:host %PROFILE% --profile:build default --build=missing
if errorlevel 1 (
    echo [ERRO] Falha ao instalar dependencias com Conan.
    exit /b 1
)

echo.
echo [2/3] Configurando e compilando com CMake...
set TOOLCHAIN=build/generators/conan_toolchain.cmake
if not exist "%TOOLCHAIN%" set TOOLCHAIN=build/%BUILD_TYPE%/generators/conan_toolchain.cmake

cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%"
if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake.
    exit /b 1
)

cmake --build build --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [ERRO] Falha na compilacao.
    exit /b 1
)

echo.
echo [3/3] Compilacao finalizada com sucesso!
echo Executavel gerado em build\%BUILD_TYPE%\escalonadorHorarios.exe ou build\escalonadorHorarios.exe
