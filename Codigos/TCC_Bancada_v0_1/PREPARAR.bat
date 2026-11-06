@echo off
setlocal
cd /d "%~dp0"
echo Preparando ambiente local com Python 3.15...
if not exist ".venv\Scripts\python.exe" py -3.15 -m venv .venv
if errorlevel 1 goto erro
".venv\Scripts\python.exe" -m pip install --only-binary=:all: -r requirements.txt
if errorlevel 1 goto erro
".venv\Scripts\python.exe" -c "import tkinter, matplotlib; print('Ambiente pronto. Abra INICIAR.bat.')"
if errorlevel 1 goto erro
pause
exit /b 0
:erro
echo Nao foi possivel preparar o ambiente. Envie uma captura do erro acima.
pause
exit /b 1
