@echo off
setlocal
cd /d "%~dp0\..\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0\AtualizarListaGenesys.ps1"
echo.
if errorlevel 1 (
  echo Nao foi possivel atualizar a lista. Confira a conexao e tente novamente.
) else (
  echo Lista Genesys atualizada. Feche e abra o EDOPro para recarregar os pontos.
)
pause
