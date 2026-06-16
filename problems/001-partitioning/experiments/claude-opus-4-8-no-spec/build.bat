@echo off
REM Build 001-partitioning solver to the only exec-allowed directory (D:\FSecret).
REM Security policy blocks running self-compiled exe elsewhere, so we emit here.
set "GXX=C:\Users\mingte.lu\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
"%GXX%" -std=c++20 -O3 -fopenmp -pthread -o "D:\FSecret\hw2.exe" "%~dp0main.cpp"
if %ERRORLEVEL% EQU 0 (
  echo SUCCESS: D:\FSecret\hw2.exe
) else (
  echo FAILED %ERRORLEVEL%
)
REM Run:   D:\FSecret\hw2.exe <input.txt> <output.out>
REM Tune:  set PART_TIME=120  (seconds of search budget; default 280)
