@echo off
setlocal
cd /d "%~dp0"
echo ==============================================
echo EDOPro Genesys - preparacao de dependencias
echo ==============================================
where git >nul 2>nul
if errorlevel 1 (
  echo ERRO: Git for Windows nao foi encontrado.
  echo Instale Git for Windows e execute este arquivo novamente.
  pause
  exit /b 1
)
if not exist "ocgcore\lua\src\lua.h" (
  echo Baixando o codigo-fonte Lua exigido pelo ocgcore...
  if exist "ocgcore\lua\src" rmdir /s /q "ocgcore\lua\src"
  git clone --depth 1 https://github.com/lua/lua.git "ocgcore\lua\src"
  if errorlevel 1 (
    echo ERRO: nao foi possivel baixar Lua. Verifique a conexao e tente novamente.
    pause
    exit /b 1
  )
) else (
  echo Codigo Lua ja encontrado.
)
echo.
echo Dependencia Lua preparada.
echo O projeto ainda exige as dependencias de compilacao do EDOPro e Visual Studio 2022.
echo Consulte BUILD_WINDOWS_PT-BR.txt antes de tentar compilar.
pause
