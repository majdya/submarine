@echo off
REM Build ground_station using MSVC cl.exe
REM Assumes MSVC is installed and environment is set up

cd %~dp0
if not exist build mkdir build
cd build

REM Compile source files
echo Compiling ground_station...
cl.exe /std:c++latest /W4 /D_WIN32 /EHsc /Fo. /Fd. ^
  ..\src\tcp_client.cpp ^
  ..\src\ground_station_cli.cpp ^
  ..\src\main.cpp ^
  /link /OUT:ground_station.exe Ws2_32.lib

if %ERRORLEVEL% EQU 0 (
  echo Build succeeded: ground_station.exe
  dir ground_station.exe
) else (
  echo Build failed with error code %ERRORLEVEL%
)
