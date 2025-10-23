@echo off
REM CASE Complete Pipeline - Build Test
REM Tests C++14 compilation with fixes

echo ========================================
echo CASE Complete Pipeline - Build Test
echo ========================================
echo.

echo [INFO] Compiling with C++14 standard...
echo.

REM Don't include CompletePipeline.hpp as a source file - it's a header
REM Don't include NativeCompiler.cpp if NativeCompiler.hpp doesn't exist

echo [1/7] Compiling Parser.cpp...
g++ -std=c++14 -O2 -c Parser.cpp -o Parser.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Parser.cpp failed
    exit /b 1
)
echo [OK] Parser.o created

echo.
echo [2/7] Compiling CodeEmitter.cpp...
g++ -std=c++14 -O2 -c CodeEmitter.cpp -o CodeEmitter.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CodeEmitter.cpp failed
    exit /b 1
)
echo [OK] CodeEmitter.o created

echo.
echo [3/7] Compiling MachineCodeEmitter.cpp...
g++ -std=c++14 -O2 -c MachineCodeEmitter.cpp -o MachineCodeEmitter.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MachineCodeEmitter.cpp failed
    exit /b 1
)
echo [OK] MachineCodeEmitter.o created

echo.
echo [4/7] Compiling CIAMCompiler.cpp...
g++ -std=c++14 -O2 -c CIAMCompiler.cpp -o CIAMCompiler.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CIAMCompiler.cpp failed
    exit /b 1
)
echo [OK] CIAMCompiler.o created

echo.
echo [5/7] Compiling OptimizationEngine.cpp...
g++ -std=c++14 -O2 -c OptimizationEngine.cpp -o OptimizationEngine.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] OptimizationEngine.cpp failed
    exit /b 1
)
echo [OK] OptimizationEngine.o created

echo.
echo [6/7] Compiling intelligence.cpp...
g++ -std=c++14 -O2 -c intelligence.cpp -o intelligence.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] intelligence.cpp failed
    exit /b 1
)
echo [OK] intelligence.o created

echo.
echo [7/7] Compiling ActiveTranspiler_Modular.cpp...
g++ -std=c++14 -O2 -c ActiveTranspiler_Modular.cpp -o ActiveTranspiler_Modular.o 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] ActiveTranspiler_Modular.cpp failed
    exit /b 1
)
echo [OK] ActiveTranspiler_Modular.o created

echo.
echo ========================================
echo Linking executable...
echo ========================================
g++ -std=c++14 -O2 Parser.o CodeEmitter.o MachineCodeEmitter.o CIAMCompiler.o OptimizationEngine.o intelligence.o ActiveTranspiler_Modular.o -o case_complete.exe 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Output: case_complete.exe
echo.
echo Run with: case_complete.exe program.case
echo.
