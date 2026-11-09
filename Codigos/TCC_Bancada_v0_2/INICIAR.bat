@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo Execute PREPARAR.bat nesta pasta primeiro.
 pause
 exit /b 1
)
".venv\Scripts\python.exe" main.py
if errorlevel 1 (
 echo O aplicativo encontrou um erro. Envie uma captura desta janela.
 pause
)
