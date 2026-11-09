@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo Execute PREPARAR.bat primeiro.
 pause
 exit /b 1
)
".venv\Scripts\python.exe" main.py --simulacao
if errorlevel 1 pause
