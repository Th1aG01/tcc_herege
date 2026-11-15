@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 goto usar_python
py -3 main.py
goto verificar

:usar_python
where python >nul 2>nul
if errorlevel 1 goto sem_python
python main.py

:verificar
if not errorlevel 1 goto fim
echo.
echo Nao foi possivel abrir a interface. Confira a mensagem acima.
pause
goto fim

:sem_python
echo Python nao encontrado. Instale o Python 3 com Tcl/Tk e adicione-o ao PATH.
pause

:fim
endlocal
