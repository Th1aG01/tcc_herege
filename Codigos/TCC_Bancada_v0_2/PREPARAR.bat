@echo off
setlocal
cd /d "%~dp0"
echo Preparando aplicativo de aquisicao TCC v0.2...
if exist ".venv\Scripts\python.exe" goto instalar
py -3 -m venv .venv
if errorlevel 1 goto erro
:instalar
".venv\Scripts\python.exe" -m pip install --only-binary=:all: -r requirements.txt
if errorlevel 1 goto erro
".venv\Scripts\python.exe" -c "import tkinter, matplotlib, serial; print('Pronto. Abra INICIAR.bat.')"
if errorlevel 1 goto erro
pause
exit /b 0
:erro
echo Nao foi possivel preparar o ambiente. Envie uma captura do erro acima.
pause
exit /b 1
