# 📖 C.A.S.E. LANGUAGE REFERENCE MANUAL

**Version 1.0** | Violet Aura Creations

---

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Syntax Overview](#syntax-overview)
4. [Data Types](#data-types)
5. [Variables](#variables)
6. [Operators](#operators)
7. [Control Flow](#control-flow)
8. [Functions](#functions)
9. [Type System](#type-system)
10. [File I/O](#file-io)
11. [Standard Library](#standard-library)
12. [Concurrency](#concurrency)
13. [Graphics & UI](#graphics--ui)
14. [Database Access](#database-access)
15. [Networking](#networking)
16. [Security & Monitoring](#security--monitoring)
17. [CIAM Features](#ciam-features)
18. [Best Practices](#best-practices)

---

## Introduction

**C.A.S.E.** (Code Assisted Syntax Evolution) is a modern, expressive programming language that transpiles to C++. It features:

- **98+ keywords** across multiple domains
- **Clean syntax** with explicit statement terminators
- **Rich type system** with type inference
- **Built-in concurrency** primitives
- **Complete standard library**
- **AI-assisted preprocessing** via CIAM
- **Beautiful error messages** with helpful suggestions

### Philosophy

C.A.S.E. is designed to be:
- **Explicit** - No hidden behavior, clear intent
- **Expressive** - Say what you mean concisely
- **Helpful** - Error messages guide you to solutions
- **Modern** - Built-in support for modern programming paradigms

---

## Getting Started

### Installation

1. Build the transpiler:
   ```bash
   # In Visual Studio
   Build > Build Solution (Ctrl+Shift+B)
   ```

2. The transpiler executable is created: `transpiler.exe`

### Your First Program

Create `hello.case`:
```case
Print "Hello, C.A.S.E.!" [end]
```

Compile and run:
```bash
transpiler.exe hello.case
./program.exe
```

**Output:**
```
Hello, C.A.S.E.!
```

---

## Syntax Overview

### Statement Terminators

**All statements must end with `[end]`**

```case
Print "Hello" [end]
let x = 5 [end]
```

### Comments

Single-line comments start with `#`:
```case
# This is a comment
Print "Code" [end]  # Inline comment
```

### Case Sensitivity

C.A.S.E. is **case-sensitive**:
- Keywords start with capital letters: `Print`, `Fn`, `let`
- Variables are typically lowercase: `myVar`, `count`

### Whitespace

Whitespace is generally ignored except for:
- Separating tokens
- Line breaks in strings (escaped)

---

## Data Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | Integer numbers | `42`, `-10` |
| `float` | Floating-point | `3.14`, `-2.5` |
| `double` | Double precision | `3.14159265` |
| `bool` | Boolean | `true`, `false` |
| `char` | Single character | (not yet supported) |
| `string` | Text strings | `"Hello"` |

### Literals

**Numbers:**
```case
let integer = 42 [end]
let decimal = 3.14 [end]
let negative = -10 [end]
```

**Strings:**
```case
let text = "Hello, World!" [end]
let escaped = "Line 1\nLine 2\tTabbed" [end]
```

**Booleans:**
```case
let yes = true [end]
let no = false [end]
```

---

## Variables

### Declaration with `let`

Use `let` for type inference:
```case
let x = 10 [end]
let name = "Alice" [end]
let pi = 3.14159 [end]
```

### Variable Assignment

```case
let x = 5 [end]
mutate x x + 1 [end]  # x becomes 6
```

### Naming Rules

- Start with letter or underscore
- Can contain letters, numbers, underscores
- Case-sensitive
- Cannot be keywords

**Valid:**
```case
let myVar = 1 [end]
let _private = 2 [end]
let value123 = 3 [end]
```

**Invalid:**
```case
let 123value = 1 [end]  # Starts with number
let Print = 2 [end]      # Keyword
```

---

## Operators

### Arithmetic Operators

| Operator | Operation | Example |
|----------|-----------|---------|
| `+` | Addition | `5 + 3` → `8` |
| `-` | Subtraction | `5 - 3` → `2` |
| `*` | Multiplication | `5 * 3` → `15` |
| `/` | Division | `6 / 3` → `2` |
| `%` | Modulo | `5 % 2` → `1` |

### Comparison Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `==` | Equal | `5 == 5` → `true` |
| `!=` | Not equal | `5 != 3` → `true` |
| `<` | Less than | `3 < 5` → `true` |
| `>` | Greater than | `5 > 3` → `true` |
| `<=` | Less or equal | `3 <= 5` → `true` |
| `>=` | Greater or equal | `5 >= 5` → `true` |

### Logical Operators

| Operator | Operation | Example |
|----------|-----------|---------|
| `&&` | Logical AND | `true && false` → `false` |
| `||` | Logical OR | `true || false` → `true` |
| `!` | Logical NOT | `!true` → `false` |

### Precedence

From highest to lowest:
1. Parentheses `()`
2. Unary operators `!`, `-`
3. Multiplication/Division `*`, `/`, `%`
4. Addition/Subtraction `+`, `-`
5. Comparison `<`, `>`, `<=`, `>=`
6. Equality `==`, `!=`
7. Logical AND `&&`
8. Logical OR `||`

---

## Control Flow

### If Statement

```case
if x > 5 {
    Print "x is greater than 5" [end]
} [end]
```

### If-Else

```case
if x > 5 {
    Print "Greater" [end]
} else {
    Print "Less or equal" [end]
} [end]
```

### While Loop

```case
let i = 0
while i < 5 {
    Print i [end]
    mutate i i + 1 [end]
} [end]
```

### Loop (For-style)

```case
loop "int i = 0; i < 10; i++" {
    Print i [end]
} [end]
```

### Break and Continue

```case
let i = 0
while i < 10 {
    if i == 5 {
        break [end]
    }
    Print i [end]
    mutate i i + 1 [end]
} [end]
```

### Switch Statement

```case
let day = 3
switch day {
    case 1 {
        Print "Monday" [end]
    }
    case 2 {
        Print "Tuesday" [end]
    }
    default {
        Print "Other day" [end]
    }
} [end]
```

---

## Functions

### Defining Functions

```case
Fn greet "name" (
    Print "Hello, " [end]
    Print name [end]
    Print "!" [end]
) [end]
```

### Calling Functions

```case
call greet "Alice" [end]
```

### Return Values

```case
Fn add "a, b" (
    ret a + b
) [end]

let result = call add 5 3 [end]
Print result [end]  # Outputs: 8
```

### Multiple Parameters

```case
Fn calculate "x, y, op" (
    if op == "+" {
        ret x + y
    } else {
        ret x - y
    }
) [end]

let sum = call calculate 10 5 "+" [end]
```

---

## Type System

### Structures

```case
struct Person {
    string name
    int age
    float height
} [end]
```

### Enumerations

```case
enum Color {
    Red,
    Green,
    Blue
} [end]
```

### Unions

```case
union Value {
    int intVal
    float floatVal
    string strVal
} [end]
```

### Type Aliases

```case
typedef int UserId [end]
typedef string Email [end]
```

---

## File I/O

### Opening Files

```case
open "data.txt" "w" fileHandle [end]
```

**Modes:**
- `"r"` - Read
- `"w"` - Write
- `"rw"` - Read/Write

### Writing to Files

```case
write fileHandle "Hello" [end]
writeln fileHandle "Hello with newline" [end]
```

### Reading from Files

```case
open "data.txt" "r" fileHandle [end]
read fileHandle content [end]
close fileHandle [end]
```

### User Input

```case
input "Enter your name: " userName [end]
Print userName [end]
```

### Serialization

```case
serialize "json" myData [end]
deserialize "json" jsonString [end]
```

### Compression

```case
compress "zlib" largeData [end]
decompress "zlib" compressedData [end]
```

---

## Standard Library

### Math Functions

```case
let angle = 45
let sine = sin angle [end]
let cosine = cos angle [end]
let tangent = tan angle [end]

let sqRoot = sqrt 16 [end]  # 4
let power = pow 2 8 [end]    # 256

let absolute = abs -5 [end]  # 5
let rounded = round 3.7 [end]  # 4
let floored = floor 3.9 [end]  # 3
let ceiled = ceil 3.1 [end]   # 4

let minimum = min 5 10 [end]  # 5
let maximum = max 5 10 [end]  # 10
let randNum = random 1 100 [end]
```

### String Functions

```case
let text = "Hello, World!"

let len = length text [end]       # 13
let sub = substr text 0 5 [end]   # "Hello"
let upper = upper text [end]      # "HELLO, WORLD!"
let lower = lower text [end]      # "hello, world!"

let combined = concat "Hello" "World" [end]
let parts = split text "," [end]
let trimmed = trim "  text  " [end]
```

### Collection Functions

```case
let nums = [1, 2, 3, 4, 5]

push nums 6 [end]              # [1,2,3,4,5,6]
pop nums [end]                 # [1,2,3,4,5]
let count = size nums [end]    # 5

sort nums [end]                # Sort in place
reverse nums [end]             # Reverse order

# Functional operations (future)
let doubled = map nums (* 2) [end]
let evens = filter nums (% 2 == 0) [end]
```

---

## Concurrency

### Threads

```case
thread {
    Print "Running in thread" [end]
} [end]
```

### Async Operations

```case
async someExpensiveOperation [end]
```

### Channels

```case
channel myChannel "int" [end]
send myChannel 42 [end]
recv myChannel result [end]
```

### Synchronization

```case
sync resource1, resource2 {
    # Critical section
    Print "Synchronized" [end]
} [end]
```

### Parallel Execution

```case
parallel {
    { Print "Task 1" [end] }
    { Print "Task 2" [end] }
    { Print "Task 3" [end] }
} [end]
```

### Batch Processing

```case
batch dataList 100 {
    # Process in batches of 100
    Print "Processing batch" [end]
} [end]
```

### Task Scheduling

```case
schedule "deferred" {
    Print "Executed later" [end]
} [end]
```

---

## Graphics & UI

### Creating Windows

```case
window "My Application" 800 600 [end]
```

### Drawing

```case
color 255 0 0 255 [end]          # Red color (RGBA)
draw "rect" 100 100 200 200 [end]  # Rectangle
draw "circle" 400 300 50 [end]     # Circle
draw "line" 0 0 800 600 [end]      # Line
render [end]                       # Display frame
```

### Widgets

```case
widget "button" myButton [end]
widget "label" statusLabel [end]
widget "textbox" inputField [end]
```

### Event Handling

```case
event "click" {
    Print "Button clicked!" [end]
} [end]
```

### Layouts

```case
layout "vertical" {
    widget "button" btn1 [end]
    widget "button" btn2 [end]
} [end]
```

---

## Database Access

### Connecting

```case
connect "sqlite" "mydb.db" db [end]
```

**Supported databases:**
- `"sqlite"`
- `"mysql"`
- `"postgres"`

### Querying

```case
query db "SELECT * FROM users" results [end]
```

### Inserting

```case
insert db "users" [end]
```

### Updating

```case
update db "users" condition [end]
```

### Deleting

```case
delete db "users" condition [end]
```

### Transactions

```case
transaction db {
    insert db "orders" [end]
    update db "inventory" condition [end]
} [end]
```

---

## Networking

### HTTP Requests

```case
http "GET" "https://api.example.com/data" response [end]
http "POST" "https://api.example.com/users" result [end]
```

### Sockets

```case
socket "tcp" "127.0.0.1" 3000 sock [end]
sendnet sock "Hello" [end]
```

### WebSockets

```case
websocket "ws://localhost:8080" ws [end]
sendnet ws "Hello" [end]
receive ws message [end]
```

### Server Listening

```case
listen server 8080 {
    Print "Request received" [end]
} [end]
```

---

## Security & Monitoring

### Memory Sanitization

```case
sanitize_mem sensitiveData [end]
san_mem password [end]  # Alias
```

### Code Sanitization

```case
sanitize_code userInput [end]
san_code scriptContent [end]  # Alias
```

### Network Diagnostics

```case
ping "google.com" "icmp" [end]
```

### System Monitoring

```case
temperature "cpu" [end]
pressure "memory" [end]
gauge "disk_usage" value [end]
```

### Auditing

```case
audit "replay" system [end]
```

### Matrix Operations

```case
matrix dataMatrix 4 4 [end]
```

---

## CIAM Features

### Enabling CIAM

```case
call CIAM[on] [end]
```

### CIAM Capabilities

- **Typo Correction**: Auto-fixes common typos
- **Print Inference**: Adds missing Print statements
- **Macro Expansion**: Expands CIAM macros
- **Base-12 Numerics**: Special numeric system
- **Code Overlay**: Runtime code modification
- **Sandbox Execution**: Safe code execution
- **Capability Auditing**: Security analysis

---

## Best Practices

### 1. Always Use `[end]`

```case
# ✅ Correct
Print "Hello" [end]

# ❌ Incorrect
Print "Hello"
```

### 2. Use Descriptive Names

```case
# ✅ Good
let userName = "Alice" [end]
let itemCount = 10 [end]

# ❌ Bad
let x = "Alice" [end]
let n = 10 [end]
```

### 3. Handle Errors Gracefully

```case
open "data.txt" "r" file [end]
if file {
    read file content [end]
    close file [end]
}
```

### 4. Comment Your Code

```case
# Calculate compound interest
let principal = 1000
let rate = 0.05
let years = 10
let amount = pow (1 + rate) years [end]
```

### 5. Use Functions for Reusability

```case
Fn calculateDiscount "price, percent" (
    ret price * (1 - percent / 100)
) [end]
```

### 6. Leverage the Standard Library

```case
# Use built-in functions
let result = sqrt 16 [end]

# Instead of implementing yourself
```

### 7. Use Concurrency Wisely

```case
# Synchronize shared resources
sync sharedData {
    mutate sharedData sharedData + 1 [end]
} [end]
```

---

## Error Messages

C.A.S.E. provides helpful error messages:

```
[Error] test.case:5:10
  Expected '[end]' to close statement

    5 | Print "Hello World"
                          ^

[Suggestion] All statements must end with [end]
```

**Common Errors:**
- Missing `[end]` terminators
- Unterminated strings
- Undefined variables
- Type mismatches (future)

---

## Appendix: Complete Keyword List

**Core (15):** Print, Fn, call, let, ret, loop, if, else, while, break, continue, switch, case, default, struct

**Type System (3):** enum, union, typedef

**File I/O (10):** open, write, writeln, read, close, input, serialize, deserialize, compress, decompress

**Security (10):** sanitize_mem, san_mem, sanitize_code, san_code, ping, audit, temperature, pressure, gauge, matrix

**Data Manipulation (4):** mutate, scale, bounds, checkpoint

**Concurrency (4):** sync, parallel, batch, schedule

**Graphics/UI (7):** window, draw, render, color, event, widget, layout

**Database (6):** connect, query, insert, update, delete, transaction

**Networking (6):** http, socket, websocket, listen, sendnet, receive

**Math (12):** sin, cos, tan, sqrt, pow, abs, floor, ceil, round, min, max, random

**Strings (10):** length, substr, concat, split, join, upper, lower, trim, replace, find

**Collections (11):** push, pop, shift, unshift, slice, map, filter, reduce, sort, reverse, size

**Total: 98+ keywords**

---

**🌌 Violet Aura Creations - C.A.S.E. Language Reference v1.0**

