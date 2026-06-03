@echo off
setlocal

set "SCRIPT_DIR=%~dp0scripts\windows"

if "%~1"=="" (
    call "%SCRIPT_DIR%\vscode-debug.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="debug" (
    call "%SCRIPT_DIR%\vscode-debug.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="dbg" (
    call "%SCRIPT_DIR%\vscode-debug.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="d" (
    call "%SCRIPT_DIR%\vscode-debug.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="release" (
    call "%SCRIPT_DIR%\vscode-release.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="rel" (
    call "%SCRIPT_DIR%\vscode-release.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="r" (
    call "%SCRIPT_DIR%\vscode-release.bat"
    exit /b %ERRORLEVEL%
)

echo Usage: start_code.bat [debug^|dbg^|d^|release^|rel^|r]
exit /b 2
