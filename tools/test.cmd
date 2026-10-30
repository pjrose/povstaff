@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>nul
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
)
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist .build mkdir .build
cl /nologo /std:c++17 /EHsc /W4 tests\sequence_format_test.cpp /Fe:.build\sequence_format_test.exe /Fo:.build\sequence_format_test.obj
if errorlevel 1 exit /b 1
.build\sequence_format_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /Itests\support tests\sequences_test.cpp code\POV4.1\Sequences.cpp /Fe:.build\sequences_test.exe /Fo:.build\
if errorlevel 1 exit /b 1
.build\sequences_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 tests\engine_math_test.cpp code\POV4.1\Patterns.cpp /Fe:.build\engine_math_test.exe /Fo:.build\
if errorlevel 1 exit /b 1
.build\engine_math_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS tests\network_validation_test.cpp /Fe:.build\network_validation_test.exe /Fo:.build\network_validation_test.obj
if errorlevel 1 exit /b 1
.build\network_validation_test.exe
if errorlevel 1 exit /b 1
node tests\dashboard_test.cjs
if errorlevel 1 exit /b 1
node tests\preview_test.cjs
if errorlevel 1 exit /b 1
echo All host tests passed.
