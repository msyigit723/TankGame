@echo off
g++ main.cpp -o TankSavas.exe -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
if %errorlevel% neq 0 (
    echo Derleme hatasi!
    pause
    exit /b %errorlevel%
)
echo Derleme basarili, oyun baslatiliyor...
TankSavas.exe
