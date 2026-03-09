@echo off
setlocal

cd /d "%~dp0"
if errorlevel 1 exit /b 1
cd testMain

echo [1/2] Compiling HL tests...
haxe test-hl.hxml
if errorlevel 1 goto :fail

echo [2/2] Running HL tests...
hl test.hl
if errorlevel 1 goto :fail

exit /b 0

:fail
echo Tests failed with exit code %ERRORLEVEL%.
exit /b %ERRORLEVEL%
