@echo off
cd /d "%~dp0"
echo Building...
g++ -std=c++20 -static-libstdc++ -I. main.cpp Backtester/Backtester.cpp Chart/Chart.cpp DataHandler/MMapFile.cpp DataHandler/DateUtils.cpp DataHandler/TapeReader.cpp -o main.exe -limgui -limplot -lglfw3 -lopengl32 -lgdi32
if errorlevel 1 exit /b 1
echo Running...
main.exe
echo Exit code: %ERRORLEVEL%
echo.
echo Files in directory:
dir output.txt 2>nul || echo output.txt NOT FOUND
dir test_output.txt 2>nul || echo test_output.txt NOT FOUND
pause
