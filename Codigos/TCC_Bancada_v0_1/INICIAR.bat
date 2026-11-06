@echo off
setlocal
cd /d "%~dp0"
set "TCC_PYTHON=%~dp0.venv\Scripts\python.exe"
if exist "%TCC_PYTHON%" goto executar
set "TCC_PYTHON=%USERPROFILE%\TCC_Bancada\.venv\Scripts\python.exe"
if exist "%TCC_PYTHON%" goto executar
echo Ambiente virtual nao encontrado. Execute PREPARAR.bat nesta pasta.
pause
exit /b 1
:executar
"%TCC_PYTHON%" main.py
if errorlevel 1 (
 echo.
 echo O programa encontrou um erro. Envie uma captura desta janela.
 pause
)
