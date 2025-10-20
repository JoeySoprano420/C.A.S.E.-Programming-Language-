# 🚀 Getting Started with C.A.S.E.

**A Beginner-Friendly Tutorial**

---

## What is C.A.S.E.?

**C.A.S.E.** (Code Assisted Syntax Evolution) is a modern programming language that makes coding fun and expressive. It transpiles to C++, giving you the power of C++ with a clean, readable syntax.

### Why C.A.S.E.?

- ✅ **Easy to learn** - Clean syntax with clear intent
- ✅ **Powerful** - 98+ keywords for everything you need
- ✅ **Helpful** - Error messages that guide you
- ✅ **Modern** - Built-in concurrency, networking, and more
- ✅ **Fast** - Compiles to optimized C++

---

## Installation

### Prerequisites

- Visual Studio 2022 (or compatible C++ compiler)
- Windows, Linux, or macOS

### Build the Transpiler

1. Open `active CASE transpiler.sln` in Visual Studio
2. Press `Ctrl+Shift+B` to build
3. Find `transpiler.exe` in the Debug or Release folder

---

## Your First Program

### Hello, World!

Create a file called `hello.case`:

```case
Print "Hello, World!" [end]
```

**Key points:**
- `Print` - Outputs text to console
- `"Hello, World!"` - String literal in quotes
- `[end]` - Every statement ends with this

### Compile and Run

```bash
transpiler.exe hello.case
./program.exe
```

**Output:**
```
Hello, World!
```

🎉 **Congratulations!** You just wrote your first C.A.S.E. program!

---

## Variables

### Creating Variables

Use `let` to create variables:

```case
let name = "Alice" [end]
let age = 25 [end]
let height = 5.6 [end]

Print name [end]    # Outputs: Alice
Print age [end]     # Outputs: 25
Print height [end]  # Outputs: 5.6
```

**Rules:**
- Variables must be declared with `let`
- Type is inferred automatically
- Names can contain letters, numbers, underscore
- Names are case-sensitive

### Modifying Variables

Use `mutate` to change values:

```case
let count = 0 [end]
Print count [end]  # 0

mutate count count + 1 [end]
Print count [end]  # 1

mutate count count * 2 [end]
Print count [end]  # 2
```

---

## Basic Math

### Arithmetic Operations

```case
let a = 10
let b = 3

Print a + b [end]  # 13 (addition)
Print a - b [end]  # 7  (subtraction)
Print a * b [end]  # 30 (multiplication)
Print a / b [end]  # 3  (division)
Print a % b [end]  # 1  (modulo/remainder)
```

### Math Functions

```case
let x = 16
Print sqrt x [end]     # 4 (square root)

let y = pow 2 8 [end]
Print y [end]          # 256 (2^8)

Print abs -5 [end]     # 5 (absolute value)
Print round 3.7 [end]  # 4 (rounding)
```

---

## Conditions

### If Statements

```case
let age = 18

if age >= 18 {
    Print "You are an adult" [end]
} [end]
```

### If-Else

```case
let score = 85

if score >= 90 {
    Print "Grade: A" [end]
} else {
    if score >= 80 {
        Print "Grade: B" [end]
    } else {
        Print "Grade: C" [end]
    }
} [end]
```

### Comparison Operators

```case
let x = 5

# Equal
if x == 5 {
    Print "x is 5" [end]
}

# Not equal
if x != 10 {
    Print "x is not 10" [end]
}

# Less than, greater than
if x < 10 {
    Print "x is less than 10" [end]
}

if x > 0 {
    Print "x is positive" [end]
}
```

---

## Loops

### While Loop

```case
let i = 1

while i <= 5 {
    Print i [end]
    mutate i i + 1 [end]
}

# Output: 1 2 3 4 5
```

### Counting Example

```case
let count = 0
let sum = 0

while count < 10 {
    mutate sum sum + count [end]
    mutate count count + 1 [end]
}

Print sum [end]  # 45 (sum of 0-9)
```

### Breaking Out

```case
let i = 0

while i < 100 {
    Print i [end]
    
    if i == 5 {
        break [end]  # Exit loop
    }
    
    mutate i i + 1 [end]
}

# Output: 0 1 2 3 4 5
```

---

## Functions

### Defining Functions

```case
Fn sayHello "name" (
    Print "Hello, " [end]
    Print name [end]
    Print "!" [end]
) [end]
```

### Calling Functions

```case
call sayHello "Alice" [end]
# Output: Hello, Alice!

call sayHello "Bob" [end]
# Output: Hello, Bob!
```

### Functions with Return Values

```case
Fn add "a, b" (
    ret a + b
) [end]

let sum = call add 5 3 [end]
Print sum [end]  # 8
```

### Multiple Parameters

```case
Fn greet "firstName, lastName" (
    Print "Welcome, " [end]
    Print firstName [end]
    Print " " [end]
    Print lastName [end]
    Print "!" [end]
) [end]

call greet "John" "Doe" [end]
# Output: Welcome, John Doe!
```

---

## Strings

### String Operations

```case
let greeting = "Hello, World!"

# Length
let len = length greeting [end]
Print len [end]  # 13

# Substring
let hello = substr greeting 0 5 [end]
Print hello [end]  # "Hello"

# Uppercase/Lowercase
let upper = upper greeting [end]
Print upper [end]  # "HELLO, WORLD!"

let lower = lower greeting [end]
Print lower [end]  # "hello, world!"
```

### Combining Strings

```case
let first = "Hello"
let second = "World"
let combined = concat first second [end]
Print combined [end]  # "HelloWorld"
```

---

## Collections

### Lists/Arrays

```case
let numbers = [1, 2, 3, 4, 5]

# Add element
push numbers 6 [end]
# Now: [1, 2, 3, 4, 5, 6]

# Get size
let count = size numbers [end]
Print count [end]  # 6

# Sort
sort numbers [end]

# Reverse
reverse numbers [end]
```

---

## File I/O

### Writing to Files

```case
# Open file for writing
open "output.txt" "w" file [end]

# Write content
writeln file "Hello, File!" [end]
writeln file "Second line" [end]

# Close file
close file [end]
```

### Reading from Files

```case
# Open file for reading
open "output.txt" "r" file [end]

# Read content
read file content [end]
Print content [end]

# Close file
close file [end]
```

### User Input

```case
input "What is your name? " userName [end]
input "How old are you? " userAge [end]

Print "Hello, " [end]
Print userName [end]
Print "! You are " [end]
Print userAge [end]
Print " years old." [end]
```

---

## Practical Examples

### Example 1: Calculator

```case
Fn calculator "a, b, operation" (
    if operation == "+" {
        ret a + b
    } else {
        if operation == "-" {
            ret a - b
        } else {
            if operation == "*" {
                ret a * b
            } else {
                ret a / b
            }
        }
    }
) [end]

let result = call calculator 10 5 "+" [end]
Print result [end]  # 15

let result2 = call calculator 10 5 "*" [end]
Print result2 [end]  # 50
```

### Example 2: Factorial

```case
Fn factorial "n" (
    if n <= 1 {
        ret 1
    } else {
        let prev = call factorial (n - 1) [end]
        ret n * prev
    }
) [end]

let result = call factorial 5 [end]
Print result [end]  # 120
```

### Example 3: Temperature Converter

```case
Fn celsiusToFahrenheit "celsius" (
    ret celsius * 9 / 5 + 32
) [end]

Fn fahrenheitToCelsius "fahrenheit" (
    ret (fahrenheit - 32) * 5 / 9
) [end]

let c = 25
let f = call celsiusToFahrenheit c [end]
Print f [end]  # 77

let f2 = 98.6
let c2 = call fahrenheitToCelsius f2 [end]
Print c2 [end]  # 37
```

### Example 4: Find Maximum

```case
Fn findMax "a, b, c" (
    let maxVal = a
    
    if b > maxVal {
        mutate maxVal b [end]
    }
    
    if c > maxVal {
        mutate maxVal c [end]
    }
    
    ret maxVal
) [end]

let maximum = call findMax 5 12 8 [end]
Print maximum [end]  # 12
```

---

## Common Mistakes

### 1. Forgetting `[end]`

❌ **Wrong:**
```case
Print "Hello"
```

✅ **Correct:**
```case
Print "Hello" [end]
```

### 2. Unterminated Strings

❌ **Wrong:**
```case
let text = "Hello
```

✅ **Correct:**
```case
let text = "Hello" [end]
```

### 3. Case Sensitivity

❌ **Wrong:**
```case
print "Hello" [end]  # lowercase 'print'
```

✅ **Correct:**
```case
Print "Hello" [end]  # Capital 'Print'
```

### 4. Missing Variable Declaration

❌ **Wrong:**
```case
x = 5
```

✅ **Correct:**
```case
let x = 5 [end]
```

---

## Next Steps

Now that you know the basics:

1. **Explore the Standard Library** - Math, strings, collections
2. **Try Concurrency** - Parallel execution, threads
3. **Build a Project** - Calculator, todo list, game
4. **Read the Language Reference** - Complete documentation
5. **Experiment** - The best way to learn!

### Resources

- **Language Reference**: `LANGUAGE_REFERENCE.md`
- **Tutorial Series**: `TUTORIALS.md`
- **Example Projects**: `examples/` folder
- **API Documentation**: Built-in help

---

## Getting Help

### Error Messages

C.A.S.E. provides helpful error messages:

```
[Error] program.case:5:10
  Expected '[end]' to close statement

    5 | Print "Hello World"
                          ^

[Suggestion] All statements must end with [end]
```

**Read the error carefully** - it tells you:
- What went wrong
- Where it happened (line and column)
- How to fix it (suggestion)

### Community

- **GitHub**: Report issues and contribute
- **Documentation**: Comprehensive guides
- **Examples**: Learn from working code

---

## Practice Exercises

Try these to solidify your knowledge:

### Exercise 1: Greeting Program

Write a program that:
1. Asks for the user's name
2. Asks for their age
3. Prints a personalized greeting

### Exercise 2: Number Guessing Game

Create a game where:
1. The program picks a random number
2. User guesses the number
3. Program gives hints (too high/too low)
4. Congratulates when correct

### Exercise 3: Grade Calculator

Write a program that:
1. Takes multiple test scores
2. Calculates the average
3. Assigns a letter grade (A-F)
4. Prints the result

---

**🎉 Congratulations on completing the Getting Started tutorial!**

You now know enough C.A.S.E. to build real programs. Keep practicing and exploring!

**🌌 Violet Aura Creations - Happy Coding!** ✨

