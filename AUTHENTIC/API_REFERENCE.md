# 📘 C.A.S.E. API DOCUMENTATION

**Complete Function Reference**

---

## Core Functions

### Print
Outputs a value to the console.

**Syntax:**
```case
Print expression [end]
```

**Example:**
```case
Print "Hello, World!" [end]
Print 42 [end]
let x = 10
Print x [end]
```

---

### let
Declares a new variable with type inference.

**Syntax:**
```case
let variableName = value [end]
```

**Example:**
```case
let name = "Alice" [end]
let age = 25 [end]
let pi = 3.14159 [end]
```

---

### Fn / call
Defines and calls functions.

**Syntax:**
```case
Fn functionName "param1, param2, ..." (
    # function body
    ret returnValue
) [end]

call functionName arg1 arg2 ... [end]
```

**Example:**
```case
Fn add "a, b" (
    ret a + b
) [end]

let sum = call add 5 3 [end]
```

---

## Math Functions

### sin, cos, tan
Trigonometric functions (radians).

**Syntax:**
```case
sin angle [end]
cos angle [end]
tan angle [end]
```

**Example:**
```case
let angle = 45
let sine = sin angle [end]
let cosine = cos angle [end]
```

---

### sqrt
Square root.

**Syntax:**
```case
sqrt number [end]
```

**Example:**
```case
let result = sqrt 16 [end]  # 4
let result2 = sqrt 2 [end]  # 1.414...
```

---

### pow
Power function (x^y).

**Syntax:**
```case
pow base exponent [end]
```

**Example:**
```case
let result = pow 2 8 [end]   # 256
let result2 = pow 3 3 [end]  # 27
```

---

### abs
Absolute value.

**Syntax:**
```case
abs number [end]
```

**Example:**
```case
let result = abs -5 [end]    # 5
let result2 = abs 10 [end]   # 10
```

---

### floor, ceil, round
Rounding functions.

**Syntax:**
```case
floor number [end]
ceil number [end]
round number [end]
```

**Example:**
```case
let f = floor 3.9 [end]   # 3
let c = ceil 3.1 [end]    # 4
let r = round 3.5 [end]   # 4
```

---

### min, max
Find minimum/maximum of two numbers.

**Syntax:**
```case
min a b [end]
max a b [end]
```

**Example:**
```case
let minimum = min 5 10 [end]  # 5
let maximum = max 5 10 [end]  # 10
```

---

### random
Generate random number in range.

**Syntax:**
```case
random min max [end]
```

**Example:**
```case
let dice = random 1 6 [end]
let percent = random 0 100 [end]
```

---

## String Functions

### length
Get string length.

**Syntax:**
```case
length string [end]
```

**Example:**
```case
let text = "Hello"
let len = length text [end]  # 5
```

---

### substr
Extract substring.

**Syntax:**
```case
substr string start length [end]
```

**Example:**
```case
let text = "Hello, World!"
let hello = substr text 0 5 [end]  # "Hello"
let world = substr text 7 5 [end]  # "World"
```

---

### upper, lower
Convert case.

**Syntax:**
```case
upper string [end]
lower string [end]
```

**Example:**
```case
let text = "Hello"
let upper = upper text [end]  # "HELLO"
let lower = lower text [end]  # "hello"
```

---

### concat
Concatenate strings.

**Syntax:**
```case
concat string1 string2 [end]
```

**Example:**
```case
let first = "Hello"
let second = "World"
let combined = concat first second [end]  # "HelloWorld"
```

---

### split
Split string into array.

**Syntax:**
```case
split string delimiter [end]
```

**Example:**
```case
let csv = "apple,banana,orange"
let fruits = split csv "," [end]
```

---

### trim
Remove whitespace.

**Syntax:**
```case
trim string [end]
```

**Example:**
```case
let text = "  Hello  "
let trimmed = trim text [end]  # "Hello"
```

---

### replace
Replace substring.

**Syntax:**
```case
replace string oldValue newValue [end]
```

**Example:**
```case
let text = "Hello World"
let result = replace text "World" "C.A.S.E." [end]
# "Hello C.A.S.E."
```

---

### find
Find substring position.

**Syntax:**
```case
find string substring [end]
```

**Example:**
```case
let text = "Hello World"
let pos = find text "World" [end]  # 6
```

---

## Collection Functions

### push
Add element to end of collection.

**Syntax:**
```case
push collection element [end]
```

**Example:**
```case
let nums = [1, 2, 3]
push nums 4 [end]
# nums is now [1, 2, 3, 4]
```

---

### pop
Remove element from end.

**Syntax:**
```case
pop collection [end]
```

**Example:**
```case
let nums = [1, 2, 3, 4]
pop nums [end]
# nums is now [1, 2, 3]
```

---

### size
Get collection size.

**Syntax:**
```case
size collection [end]
```

**Example:**
```case
let nums = [1, 2, 3, 4, 5]
let count = size nums [end]  # 5
```

---

### sort
Sort collection in place.

**Syntax:**
```case
sort collection [end]
```

**Example:**
```case
let nums = [5, 2, 8, 1, 9]
sort nums [end]
# nums is now [1, 2, 5, 8, 9]
```

---

### reverse
Reverse collection order.

**Syntax:**
```case
reverse collection [end]
```

**Example:**
```case
let nums = [1, 2, 3, 4, 5]
reverse nums [end]
# nums is now [5, 4, 3, 2, 1]
```

---

## File I/O Functions

### open
Open a file.

**Syntax:**
```case
open filename mode handle [end]
```

**Modes:**
- `"r"` - Read
- `"w"` - Write
- `"rw"` - Read/Write

**Example:**
```case
open "data.txt" "w" file [end]
```

---

### write, writeln
Write to file.

**Syntax:**
```case
write handle content [end]
writeln handle content [end]
```

**Example:**
```case
write file "Hello" [end]
writeln file "Hello with newline" [end]
```

---

### read
Read from file.

**Syntax:**
```case
read handle variable [end]
```

**Example:**
```case
read file content [end]
Print content [end]
```

---

### close
Close file.

**Syntax:**
```case
close handle [end]
```

**Example:**
```case
close file [end]
```

---

### input
Get user input.

**Syntax:**
```case
input prompt variable [end]
```

**Example:**
```case
input "Enter your name: " name [end]
input "Enter your age: " age [end]
```

---

## Concurrency Functions

### thread
Create new thread.

**Syntax:**
```case
thread {
    # code to run in thread
} [end]
```

**Example:**
```case
thread {
    Print "Running in background" [end]
} [end]
```

---

### parallel
Execute tasks in parallel.

**Syntax:**
```case
parallel {
    { task1 }
    { task2 }
    { task3 }
} [end]
```

**Example:**
```case
parallel {
    { Print "Task 1" [end] }
    { Print "Task 2" [end] }
} [end]
```

---

### sync
Synchronized block.

**Syntax:**
```case
sync resource1, resource2 {
    # critical section
} [end]
```

**Example:**
```case
sync counter {
    mutate counter counter + 1 [end]
} [end]
```

---

## Database Functions

### connect
Connect to database.

**Syntax:**
```case
connect type connectionString handle [end]
```

**Types:**
- `"sqlite"`
- `"mysql"`
- `"postgres"`

**Example:**
```case
connect "sqlite" "mydb.db" db [end]
```

---

### query
Execute SQL query.

**Syntax:**
```case
query handle sql resultVariable [end]
```

**Example:**
```case
query db "SELECT * FROM users" results [end]
```

---

## Networking Functions

### http
Make HTTP request.

**Syntax:**
```case
http method url resultVariable [end]
```

**Methods:**
- `"GET"`
- `"POST"`
- `"PUT"`
- `"DELETE"`

**Example:**
```case
http "GET" "https://api.example.com/data" response [end]
```

---

### socket
Create socket connection.

**Syntax:**
```case
socket type address port handle [end]
```

**Types:**
- `"tcp"`
- `"udp"`

**Example:**
```case
socket "tcp" "127.0.0.1" 3000 sock [end]
```

---

### websocket
Create WebSocket connection.

**Syntax:**
```case
websocket url handle [end]
```

**Example:**
```case
websocket "ws://localhost:8080" ws [end]
```

---

## Graphics Functions

### window
Create window.

**Syntax:**
```case
window title width height [end]
```

**Example:**
```case
window "My App" 800 600 [end]
```

---

### draw
Draw shape.

**Syntax:**
```case
draw shape parameters... [end]
```

**Shapes:**
- `"rect"` - Rectangle (x, y, width, height)
- `"circle"` - Circle (x, y, radius)
- `"line"` - Line (x1, y1, x2, y2)

**Example:**
```case
draw "rect" 100 100 200 200 [end]
draw "circle" 400 300 50 [end]
```

---

### color
Set drawing color.

**Syntax:**
```case
color r g b a [end]
```

**Parameters:**
- r, g, b: 0-255
- a: 0-255 (alpha/transparency)

**Example:**
```case
color 255 0 0 255 [end]  # Red
draw "rect" 100 100 50 50 [end]
```

---

### render
Render frame.

**Syntax:**
```case
render [end]
```

**Example:**
```case
draw "rect" 100 100 200 200 [end]
render [end]
```

---

## Security Functions

### sanitize_mem
Sanitize memory.

**Syntax:**
```case
sanitize_mem variable [end]
san_mem variable [end]  # Alias
```

**Example:**
```case
sanitize_mem password [end]
```

---

### sanitize_code
Sanitize code.

**Syntax:**
```case
sanitize_code code [end]
san_code code [end]  # Alias
```

**Example:**
```case
sanitize_code userInput [end]
```

---

**🌌 Violet Aura Creations - Complete API Reference**

