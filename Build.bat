@echo off
REM Criar pasta build
if not exist build mkdir build
cd build

REM Configurar o projeto para Visual Studio 2026 x64
cmake .. -G "Visual Studio 18 2026" -A x64

REM Build em modo Release
cmake --build . --config Release

pause
