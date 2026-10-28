@echo off
rem Build with Visual Studio (run from "x64 Native Tools Command Prompt")
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build || exit /b 1
echo.
echo Done: build\milleipswitch.exe
