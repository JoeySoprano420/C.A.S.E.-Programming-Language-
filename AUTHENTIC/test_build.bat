@echo off
REM CASE CIAM AOT Compiler - Compilation Test Script
REM Tests C++14 compatibility

echo ========================================
echo CASE CIAM AOT Compiler - Build Test
echo ========================================
echo.

echo [1/4] Compiling CIAMCompiler.cpp...
g++ -std=c++14 -c CIAMCompiler.cpp -o CIAMCompiler.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CIAMCompiler.cpp failed to compile
    exit /b 1
)
echo SUCCESS: CIAMCompiler.o created

echo.
echo [2/4] Compiling MachineCodeEmitter.cpp...
g++ -std=c++14 -c MachineCodeEmitter.cpp -o MachineCodeEmitter.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: MachineCodeEmitter.cpp failed to compile
    exit /b 1
)
echo SUCCESS: MachineCodeEmitter.o created

echo.
echo [3/4] Compiling CodeEmitter.cpp...
g++ -std=c++14 -c CodeEmitter.cpp -o CodeEmitter.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CodeEmitter.cpp failed to compile
    exit /b 1
)
echo SUCCESS: CodeEmitter.o created

echo.
echo [4/4] Compiling Parser.cpp...
g++ -std=c++14 -c Parser.cpp -o Parser.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Parser.cpp failed to compile
    exit /b 1
)
echo SUCCESS: Parser.o created

echo.
echo ========================================
echo ALL COMPILATION TESTS PASSED!
echo ========================================
echo.
echo Object files created:
echo   - CIAMCompiler.o
echo   - MachineCodeEmitter.o
echo   - CodeEmitter.o
echo   - Parser.o
echo.
echo Next step: Link into executable with:
echo   g++ -std=c++14 *.o -o case_compiler.exe
echo.
