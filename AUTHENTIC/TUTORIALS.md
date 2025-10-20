# 📚 C.A.S.E. TUTORIAL SERIES

**Learn by Building Real Projects**

---

## Tutorial Index

1. [Tutorial 1: Hello World & Variables](#tutorial-1-hello-world--variables)
2. [Tutorial 2: Control Flow & Loops](#tutorial-2-control-flow--loops)
3. [Tutorial 3: Functions & Modularity](#tutorial-3-functions--modularity)
4. [Tutorial 4: Working with Files](#tutorial-4-working-with-files)
5. [Tutorial 5: Collections & Data Structures](#tutorial-5-collections--data-structures)
6. [Tutorial 6: Building a Calculator](#tutorial-6-building-a-calculator)
7. [Tutorial 7: Todo List Application](#tutorial-7-todo-list-application)
8. [Tutorial 8: Concurrency Basics](#tutorial-8-concurrency-basics)
9. [Tutorial 9: Database Integration](#tutorial-9-database-integration)
10. [Tutorial 10: Building a Web Server](#tutorial-10-building-a-web-server)

---

## Tutorial 1: Hello World & Variables

**Goal:** Learn basic syntax and variable handling

### Step 1: Print Statements

```case
Print "Hello, C.A.S.E.!" [end]
Print "Welcome to programming!" [end]
```

### Step 2: Variables

```case
let message = "Hello, World!" [end]
Print message [end]

let number = 42 [end]
Print number [end]

let pi = 3.14159 [end]
Print pi [end]
```

### Step 3: Variable Operations

```case
let x = 10
let y = 5

let sum = x + y
Print sum [end]  # 15

let product = x * y
Print product [end]  # 50

let quotient = x / y
Print quotient [end]  # 2
```

### Challenge

Create a program that:
1. Declares your name, age, and favorite number
2. Prints them in a formatted message
3. Performs a calculation with the favorite number

---

## Tutorial 2: Control Flow & Loops

**Goal:** Master conditions and iterations

### If Statements

```case
let age = 18

if age >= 18 {
    Print "You can vote!" [end]
} else {
    Print "Too young to vote" [end]
} [end]
```

### While Loops

```case
let count = 1

while count <= 5 {
    Print count [end]
    mutate count count + 1 [end]
}
```

### FizzBuzz Challenge

```case
let n = 1

while n <= 20 {
    if n % 15 == 0 {
        Print "FizzBuzz" [end]
    } else {
        if n % 3 == 0 {
            Print "Fizz" [end]
        } else {
            if n % 5 == 0 {
                Print "Buzz" [end]
            } else {
                Print n [end]
            }
        }
    }
    mutate n n + 1 [end]
}
```

### Project: Number Guessing Game

```case
let secret = 7
let guess = 0
let attempts = 0

input "Guess a number (1-10): " guess [end]

while guess != secret {
    mutate attempts attempts + 1 [end]
    
    if guess < secret {
        Print "Too low!" [end]
    } else {
        Print "Too high!" [end]
    }
    
    input "Try again: " guess [end]
}

Print "Correct! Attempts: " [end]
Print attempts [end]
```

---

## Tutorial 3: Functions & Modularity

**Goal:** Write reusable, organized code

### Basic Functions

```case
Fn greet "name" (
    Print "Hello, " [end]
    Print name [end]
    Print "!" [end]
) [end]

call greet "Alice" [end]
call greet "Bob" [end]
```

### Functions with Return Values

```case
Fn square "x" (
    ret x * x
) [end]

Fn cube "x" (
    let sq = call square x [end]
    ret sq * x
) [end]

let result = call cube 3 [end]
Print result [end]  # 27
```

### Project: Math Library

```case
Fn add "a, b" (
    ret a + b
) [end]

Fn subtract "a, b" (
    ret a - b
) [end]

Fn multiply "a, b" (
    ret a * b
) [end]

Fn divide "a, b" (
    if b == 0 {
        Print "Error: Division by zero!" [end]
        ret 0
    }
    ret a / b
) [end]

# Test the library
let sum = call add 10 5 [end]
let diff = call subtract 10 5 [end]
let prod = call multiply 10 5 [end]
let quot = call divide 10 5 [end]

Print sum [end]   # 15
Print diff [end]  # 5
Print prod [end]  # 50
Print quot [end]  # 2
```

---

## Tutorial 4: Working with Files

**Goal:** Read and write data to files

### Writing to Files

```case
open "diary.txt" "w" file [end]

writeln file "Day 1: Started learning C.A.S.E." [end]
writeln file "Day 2: Built my first calculator." [end]
writeln file "Day 3: Created a file handling program!" [end]

close file [end]
Print "Diary saved!" [end]
```

### Reading from Files

```case
open "diary.txt" "r" file [end]
read file content [end]
close file [end]

Print "Diary contents:" [end]
Print content [end]
```

### Project: Contact Book

```case
Fn saveContact "name, phone" (
    open "contacts.txt" "w" file [end]
    writeln file name [end]
    writeln file phone [end]
    close file [end]
    Print "Contact saved!" [end]
) [end]

Fn loadContact "" (
    open "contacts.txt" "r" file [end]
    read file name [end]
    read file phone [end]
    close file [end]
    
    Print "Name: " [end]
    Print name [end]
    Print "Phone: " [end]
    Print phone [end]
) [end]

# Save a contact
call saveContact "Alice" "555-1234" [end]

# Load it back
call loadContact [end]
```

---

## Tutorial 5: Collections & Data Structures

**Goal:** Work with lists and collections

### Basic List Operations

```case
let numbers = [1, 2, 3, 4, 5]

# Add element
push numbers 6 [end]
Print numbers [end]

# Get size
let count = size numbers [end]
Print count [end]  # 6

# Sort
sort numbers [end]

# Reverse
reverse numbers [end]
```

### Project: Student Grade Manager

```case
let students = ["Alice", "Bob", "Charlie"]
let grades = [85, 92, 78]

# Add new student
push students "Diana" [end]
push grades 95 [end]

# Find average
let sum = 0
let i = 0

while i < size grades [end] {
    let grade = grades[i]
    mutate sum sum + grade [end]
    mutate i i + 1 [end]
}

let average = sum / size grades [end]
Print "Average grade: " [end]
Print average [end]

# Sort by grade (conceptual)
sort grades [end]
Print "Sorted grades:" [end]
Print grades [end]
```

---

## Tutorial 6: Building a Calculator

**Goal:** Create a full-featured calculator

```case
Fn calculator "a, b, op" (
    if op == "+" {
        ret a + b
    } else {
        if op == "-" {
            ret a - b
        } else {
            if op == "*" {
                ret a * b
            } else {
                if op == "/" {
                    if b == 0 {
                        Print "Error: Division by zero!" [end]
                        ret 0
                    }
                    ret a / b
                } else {
                    if op == "^" {
                        ret pow a b [end]
                    } else {
                        if op == "%" {
                            ret a % b
                        } else {
                            Print "Unknown operation!" [end]
                            ret 0
                        }
                    }
                }
            }
        }
    }
) [end]

Fn showMenu "" (
    Print "=== Calculator ===" [end]
    Print "Operations: +, -, *, /, ^, %" [end]
) [end]

# Main program
call showMenu [end]

input "Enter first number: " num1 [end]
input "Enter operation: " operation [end]
input "Enter second number: " num2 [end]

let result = call calculator num1 num2 operation [end]

Print "Result: " [end]
Print result [end]
```

---

## Tutorial 7: Todo List Application

**Goal:** Build a complete todo list manager

```case
# Global todo list
let todos = []
let completed = []

Fn addTodo "task" (
    push todos task [end]
    Print "Task added: " [end]
    Print task [end]
) [end]

Fn listTodos "" (
    Print "=== Todo List ===" [end]
    let i = 0
    while i < size todos [end] {
        Print i + 1 [end]
        Print ". " [end]
        Print todos[i] [end]
        mutate i i + 1 [end]
    }
) [end]

Fn completeTodo "index" (
    let task = todos[index]
    push completed task [end]
    # Remove from todos (conceptual)
    Print "Completed: " [end]
    Print task [end]
) [end]

Fn showMenu "" (
    Print "1. Add Task" [end]
    Print "2. List Tasks" [end]
    Print "3. Complete Task" [end]
    Print "4. Exit" [end]
) [end]

# Main loop
let running = true

while running {
    call showMenu [end]
    input "Choice: " choice [end]
    
    if choice == 1 {
        input "Enter task: " task [end]
        call addTodo task [end]
    } else {
        if choice == 2 {
            call listTodos [end]
        } else {
            if choice == 3 {
                input "Task number: " index [end]
                call completeTodo index - 1 [end]
            } else {
                if choice == 4 {
                    mutate running false [end]
                }
            }
        }
    }
}
```

---

## Tutorial 8: Concurrency Basics

**Goal:** Learn parallel programming

### Basic Threading

```case
thread {
    Print "Thread 1 running" [end]
} [end]

thread {
    Print "Thread 2 running" [end]
} [end]

Print "Main thread continues" [end]
```

### Parallel Execution

```case
parallel {
    {
        Print "Task 1: Processing..." [end]
        # Simulate work
    }
    {
        Print "Task 2: Processing..." [end]
        # Simulate work
    }
    {
        Print "Task 3: Processing..." [end]
        # Simulate work
    }
} [end]

Print "All tasks completed!" [end]
```

### Synchronized Access

```case
let counter = 0

sync counter {
    mutate counter counter + 1 [end]
} [end]

Print counter [end]
```

---

## Tutorial 9: Database Integration

**Goal:** Store and retrieve data from databases

### Basic Database Operations

```case
# Connect to database
connect "sqlite" "myapp.db" db [end]

# Create table (conceptual SQL)
query db "CREATE TABLE users (id INTEGER, name TEXT)" results [end]

# Insert data
insert db "users" [end]

# Query data
query db "SELECT * FROM users" users [end]

# Display results
Print "Users:" [end]
Print users [end]
```

### Project: Simple User Database

```case
Fn initDatabase "" (
    connect "sqlite" "users.db" db [end]
    query db "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)" results [end]
) [end]

Fn addUser "name, email" (
    connect "sqlite" "users.db" db [end]
    # Insert user (conceptual)
    insert db "users" [end]
    Print "User added: " [end]
    Print name [end]
) [end]

Fn listUsers "" (
    connect "sqlite" "users.db" db [end]
    query db "SELECT * FROM users" users [end]
    Print "All users:" [end]
    Print users [end]
) [end]

# Initialize
call initDatabase [end]

# Add users
call addUser "Alice" "alice@example.com" [end]
call addUser "Bob" "bob@example.com" [end]

# List all users
call listUsers [end]
```

---

## Tutorial 10: Building a Web Server

**Goal:** Create a simple HTTP server

```case
Fn handleRequest "request" (
    Print "Received request" [end]
    Print request [end]
    
    # Send response
    ret "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Hello from C.A.S.E.!</h1>"
) [end]

Fn startServer "port" (
    Print "Starting server on port " [end]
    Print port [end]
    
    listen server port {
        let response = call handleRequest request [end]
        sendnet server response [end]
    } [end]
) [end]

# Start the server
call startServer 8080 [end]
Print "Server running at http://localhost:8080" [end]
```

---

## Practice Projects

### Beginner Projects
1. **Temperature Converter** - Celsius to Fahrenheit and vice versa
2. **Simple Calculator** - Basic arithmetic operations
3. **Guessing Game** - Computer picks a number, user guesses
4. **Rock Paper Scissors** - Classic game against computer

### Intermediate Projects
5. **Contact Manager** - Store and search contacts
6. **Shopping List** - Add, remove, and check off items
7. **Expense Tracker** - Track income and expenses
8. **Password Generator** - Create random secure passwords

### Advanced Projects
9. **Chat Application** - Multi-user chat with networking
10. **Blog Platform** - Create, edit, and display blog posts
11. **File Organizer** - Automatically organize files by type
12. **Game Engine** - Simple 2D game with graphics

---

## Next Steps

After completing these tutorials:

1. **Build Your Own Project** - Apply what you've learned
2. **Explore Advanced Features** - Graphics, networking, concurrency
3. **Contribute** - Share your projects with the community
4. **Teach Others** - Best way to solidify knowledge

---

## Resources

- **Language Reference**: Complete keyword documentation
- **API Documentation**: Detailed function reference
- **Example Code**: Working programs to learn from
- **Community**: Get help and share knowledge

---

**🎉 Congratulations on completing the tutorial series!**

You're now ready to build real-world applications with C.A.S.E.!

**🌌 Violet Aura Creations - Keep Building!** ✨

