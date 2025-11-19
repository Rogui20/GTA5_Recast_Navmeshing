@echo off
REM Criar pasta build
if not exist build mkdir build
cd build

REM Configurar o projeto para Visual Studio 2022 x64
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build em modo Release
cmake --build . --config Release

pause
