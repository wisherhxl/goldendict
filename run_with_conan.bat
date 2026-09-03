@echo off
setlocal

python "%~dp0scripts\run_with_conan.py" %*
exit /b %ERRORLEVEL%
