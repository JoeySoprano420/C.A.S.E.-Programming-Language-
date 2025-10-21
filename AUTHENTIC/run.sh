#!/bin/bash
# ============================================================================
# C.A.S.E. Transpiler - Automated Build and Run Script (Linux/macOS)
# ============================================================================

TRANSPILER="./transpiler"
COMPILER="clang++"
STD="c++17"
OPT="O2"
OUTPUT="program"
AUTO_RUN=1
VERBOSE=1

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
NC='\033[0m' # No Color

# Parse command-line arguments
INPUT_FILE=""
while [[ $# -gt 0 ]]; do
    case $1 in
   --no-run)
   AUTO_RUN=0
          shift
        ;;
      --quiet)
            VERBOSE=0
            shift
 ;;
        --compiler)
        COMPILER="$2"
       shift 2
          ;;
        --std)
            STD="$2"
  shift 2
   ;;
        --opt)
            OPT="$2"
 shift 2
            ;;
     --output)
          OUTPUT="$2"
        shift 2
            ;;
     -h|--help)
     echo "Usage: ./run.sh <input.case> [options]"
            echo ""
      echo "Options:"
            echo "  --no-run      Compile but don't execute"
    echo "  --compiler <name>   Specify compiler (default: clang++)"
 echo "  --std <version>     C++ standard (default: c++17)"
 echo "  --opt <level>       Optimization level (default: O2)"
       echo "  --output <name>Output executable name (default: program)"
  echo "  --quiet     Suppress verbose output"
      echo "  -h, --helpShow this help message"
            echo ""
            echo "Examples:"
    echo "  ./run.sh hello.case"
   echo "  ./run.sh test.case --no-run"
 echo "  ./run.sh program.case --compiler g++ --std c++20"
     exit 0
    ;;
        *)
      if [ -z "$INPUT_FILE" ]; then
     INPUT_FILE="$1"
            fi
   shift
          ;;
    esac
done

# Check input file
if [ -z "$INPUT_FILE" ]; then
 echo -e "${RED}[Error]${NC} No input file specified"
    echo "Usage: ./run.sh <input.case> [options]"
    echo "Use --help for more information"
exit 1
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${RED}[Error]${NC} File not found: $INPUT_FILE"
    exit 1
fi

# Display header
if [ $VERBOSE -eq 1 ]; then
    echo "============================================================================"
    echo "  C.A.S.E. Transpiler - Automated Compilation"
    echo "============================================================================"
    echo ""
    echo "Input:    $INPUT_FILE"
    echo "Compiler: $COMPILER"
    echo "Standard: $STD"
    echo "Optimize: $OPT"
    echo "Output:   $OUTPUT"
    echo ""
fi

# Step 1: Transpile C.A.S.E. to C++
if [ $VERBOSE -eq 1 ]; then
    echo -e "${CYAN}[Step 1]${NC} Transpiling C.A.S.E. to C++..."
fi

$TRANSPILER "$INPUT_FILE"
if [ $? -ne 0 ]; then
    echo ""
    echo -e "${RED}[Error]${NC} Transpilation failed!"
    exit 1
fi

if [ $VERBOSE -eq 1 ]; then
    echo -e "${GREEN}[Success]${NC} Generated compiler.cpp"
    echo ""
fi

# Step 2: Compile C++ to native executable
if [ $VERBOSE -eq 1 ]; then
    echo -e "${CYAN}[Step 2]${NC} Compiling C++ to native code..."
fi

$COMPILER -std=$STD -$OPT compiler.cpp -o $OUTPUT 2> compile_errors.txt
if [ $? -ne 0 ]; then
    echo ""
    echo -e "${RED}[Error]${NC} C++ compilation failed!"
    echo "Check compile_errors.txt for details:"
    cat compile_errors.txt
    exit 1
fi

if [ $VERBOSE -eq 1 ]; then
    echo -e "${GREEN}[Success]${NC} Compiled to $OUTPUT"
    echo ""
fi

rm -f compile_errors.txt

# Step 3: Run the program (if enabled)
if [ $AUTO_RUN -eq 1 ]; then
    if [ $VERBOSE -eq 1 ]; then
    echo -e "${CYAN}[Step 3]${NC} Running $OUTPUT..."
        echo "============================================================================"
        echo ""
    fi
    
    ./$OUTPUT
    EXIT_CODE=$?
    
    echo ""
    if [ $VERBOSE -eq 1 ]; then
        echo "============================================================================"
        if [ $EXIT_CODE -eq 0 ]; then
         echo -e "${GREEN}[Success]${NC} Program executed successfully (exit code: 0)"
        else
  echo -e "${YELLOW}[Warning]${NC} Program exited with code: $EXIT_CODE"
        fi
    fi
    
    exit $EXIT_CODE
else
    if [ $VERBOSE -eq 1 ]; then
      echo -e "${CYAN}[Step 3]${NC} Skipping execution (--no-run specified)"
        echo ""
echo "To run manually: ./$OUTPUT"
    fi
    exit 0
fi
