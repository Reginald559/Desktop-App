ECHO off
ECHO ===== Installing CMake =====

set CMAKE_TEMP="C:\cmake_temp"

rd /s /q %CMAKE_TEMP%

mkdir %CMAKE_TEMP%
curl.exe https://github.com/Kitware/CMake/releases/download/v3.23.2/cmake-3.23.2-windows-x86_64.msi -o %CMAKE_TEMP%\cmake-3.23.2-windows-x86_64.msi -k -L
call %CMAKE_TEMP%\cmake-3.23.2-windows-x86_64.msi /passive

rd /s /q %CMAKE_TEMP%
