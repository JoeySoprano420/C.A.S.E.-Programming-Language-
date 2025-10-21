@echo off
REM ============================================================================
REM C.A.S.E. Transpiler - Automated Build and Run Script
REM ============================================================================

setlocal enabledelayedexpansion

set TRANSPILER=transpiler.exe
set INPUT_FILE=%1
set COMPILER=clang++
set STD=c++17
set OPT=O2
set OUTPUT=program.exe
set AUTO_RUN=1
set VERBOSE=1

REM Parse command-line arguments
:parse_args
if "%2"=="" goto check_input
if "%2"=="--no-run" set AUTO_RUN=0
if "%2"=="--quiet" set VERBOSE=0
if "%2"=="--compiler" (
    set COMPILER=%3
    shift
)
if "%2"=="--std" (
    set STD=%3
    shift
)
if "%2"=="--opt" (
    set OPT=%3
    shift
)
if "%2"=="--output" (
    set OUTPUT=%3
    shift
)
shift
goto parse_args

:check_input
if "%INPUT_FILE%"=="" (
    echo Usage: run.bat ^<input.case^> [options]
    echo.
    echo Options:
    echo   --no-run   Compile but don't execute
    echo   --compiler ^<name^>  Specify compiler ^(default: clang++^)
    echo   --std ^<version^>    C++ standard ^(default: c++17^)
    echo   --opt ^<level^>      Optimization level ^(default: O2^)
    echo   --output ^<name^>    Output executable name ^(default: program.exe^)
    echo   --quiet            Suppress verbose output
    echo.
    echo Examples:
    echo   run.bat hello.case
    echo   run.bat test.case --no-run
    echo   run.bat program.case --compiler g++ --std c++20
    exit /b 1
)

if not exist "%INPUT_FILE%" (
    echo [Error] File not found: %INPUT_FILE%
    exit /b 1
)

REM Display header
if %VERBOSE%==1 (
    echo ============================================================================
    echo   C.A.S.E. Transpiler - Automated Compilation
    echo ============================================================================
    echo.
    echo Input:    %INPUT_FILE%
    echo Compiler: %COMPILER%
    echo Standard: %STD%
    echo Optimize: %OPT%
    echo Output:   %OUTPUT%
    echo.
)

REM Step 1: Transpile C.A.S.E. to C++
if %VERBOSE%==1 (
    echo [Step 1] Transpiling C.A.S.E. to C++...
)

%TRANSPILER% "%INPUT_FILE%"
if errorlevel 1 (
    echo.
    echo [Error] Transpilation failed!
    exit /b 1
)

if %VERBOSE%==1 (
    echo [Success] Generated compiler.cpp
    echo.
)

REM Step 2: Compile C++ to native executable
if %VERBOSE%==1 (
    echo [Step 2] Compiling C++ to native code...
)

%COMPILER% -std=%STD% -%OPT% compiler.cpp -o %OUTPUT% 2> compile_errors.txt
if errorlevel 1 (
    echo.
    echo [Error] C++ compilation failed!
    echo Check compile_errors.txt for details
    type compile_errors.txt
    exit /b 1
)

if %VERBOSE%==1 (
    echo [Success] Compiled to %OUTPUT%
    echo.
)

del compile_errors.txt 2>nul

REM Step 3: Run the program (if enabled)
if %AUTO_RUN%==1 (
    if %VERBOSE%==1 (
        echo [Step 3] Running %OUTPUT%...
        echo ============================================================================
        echo.
    )
    
    %OUTPUT%
 set EXIT_CODE=!errorlevel!
    
    echo.
  if %VERBOSE%==1 (
        echo ============================================================================
  if !EXIT_CODE!==0 (
      echo [Success] Program executed successfully ^(exit code: 0^)
     ) else (
            echo [Warning] Program exited with code: !EXIT_CODE!
   )
    )
    
    exit /b !EXIT_CODE!
) else (
    if %VERBOSE%==1 (
     echo [Step 3] Skipping execution (--no-run specified)
      echo.
  echo To run manually: %OUTPUT%
    )
  exit /b 0
)
