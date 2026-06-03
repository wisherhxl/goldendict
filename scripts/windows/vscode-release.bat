@echo off
setlocal

set "REPO_ROOT=%~dp0..\.."
set "BUILD_TYPE=Release"

where conan >nul 2>nul
if ERRORLEVEL 1 (
    echo Conan was not found in PATH.
    exit /b 1
)

where code >nul 2>nul
if ERRORLEVEL 1 (
    echo VS Code command 'code' was not found in PATH.
    exit /b 1
)

pushd "%REPO_ROOT%" >nul

conan install . --build=missing -s build_type=%BUILD_TYPE%
if ERRORLEVEL 1 (
    popd >nul
    exit /b 1
)

set "GENERATORS_DIR=%REPO_ROOT%\build\generators"
if not exist "%GENERATORS_DIR%\" if exist "%REPO_ROOT%\build\%BUILD_TYPE%\generators\" set "GENERATORS_DIR=%REPO_ROOT%\build\%BUILD_TYPE%\generators"

if exist "%GENERATORS_DIR%\deactivate_conanrun.bat" call "%GENERATORS_DIR%\deactivate_conanrun.bat"
if exist "%GENERATORS_DIR%\deactivate_conanbuild.bat" call "%GENERATORS_DIR%\deactivate_conanbuild.bat"

if exist "%GENERATORS_DIR%\conanbuild.bat" call "%GENERATORS_DIR%\conanbuild.bat"
if exist "%GENERATORS_DIR%\conanrun.bat" call "%GENERATORS_DIR%\conanrun.bat"

code "%REPO_ROOT%"
set "EXIT_CODE=%ERRORLEVEL%"

popd >nul
exit /b %EXIT_CODE%
