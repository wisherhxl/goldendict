@echo off
setlocal

set "SCRIPT_DIR=%~dp0scripts\windows"

if "%~1"=="" (
    call "%SCRIPT_DIR%\vscode-debug.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="-rel" (
    call "%SCRIPT_DIR%\vscode-release.bat"
    exit /b %ERRORLEVEL%
)

if /I "%~1"=="--release" (
    call "%SCRIPT_DIR%\vscode-release.bat"
    exit /b %ERRORLEVEL%
)

echo Usage: vscode.bat [-rel]
exit /b 2
