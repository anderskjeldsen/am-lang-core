# AmLang AI Assistant Guide

This file contains comprehensive information about the AmLang programming language to help AI assistants provide accurate code suggestions and support.

## Language Overview

AmLang is a modern, statically-typed programming language that compiles to native binaries. It combines object-oriented and functional programming paradigms with strong type safety and native integration capabilities.

## Core Syntax and Features

### Basic Structure
```amlang
namespace MyProject {
    class App {
        fun main() {
            "Hello, World!".println()
        }
    }
}
```

### Variable Declarations
```amlang
// Type inference
var name = "Anders"
var age = 30
var pi = 3.14159

// Explicit types
var count: Int = 0
var message: String = "Hello"
var isActive: Bool = true

// Constants (const var - partially implemented)
const var MAX_SIZE = 100

// Type-specific constants
var shortValue: Short = 5S
var ushortValue: UShort = 10US
var longValue: Long = 1000L
var ulongValue: ULong = 2000UL
var floatValue: Float = 3.14F
var doubleValue: Double = 3.14159D
```

### Functions
```amlang
// Basic function
fun greet(name: String): String {
    return "Hello, ${name}!"
}

// Function with default parameters
fun calculate(x: Int, y: Int = 10): Int {
    return x + y
}

// Lambda/Anonymous functions (parameters are type-inferred)
var square: (Int) => Int = (x) => x * x
var add: (Int, Int) => Int = (a, b) => a + b
```

### Classes and Objects
```amlang
// Imports are done at class level for easy refactoring
import Am.Collections.List

// Basic class with primary constructor (only constructor type available)
class Person(private var name: String, private var age: Int) {
    fun getName(): String {
        return this.name
    }
    
    fun setAge(newAge: Int) {
        this.age = newAge
    }
    
    // For additional construction needs, use static factory functions
    static fun createChild(name: String): Person {
        return new Person(name, 0)
    }
    
    static fun createAdult(name: String): Person {
        return new Person(name, 18)
    }
    
    override fun toString(): String {
        return "Person(${this.name}, ${this.age})"
    }
}

// Creating instances requires 'new' keyword
var person = new Person("Alice", 25)
var child = Person.createChild("Bob")

// Inheritance
class Student(name: String, age: Int, private var studentId: String) : Person(name, age) {
    fun getStudentId(): String {
        return this.studentId
    }
}

var student = new Student("Charlie", 20, "S12345")
```

### Native Classes (for system integration)
```amlang
import Am.Lang.String

native class FileSystem {
    native fun readFile(path: String): String
    native fun writeFile(path: String, content: String): Bool
    native fun fileExists(path: String): Bool
    
    // Implementation wrapper functions can call native implementations
    fun readFileWithErrorHandling(path: String): String {
        if (this.fileExists(path)) {
            return this.readFile(path)
        } else {
            return ""
        }
    }
}

// Native function implementation in C (FileSystem.c)
/*
function_result Example_FileSystem_readFile_0(aobject * const this, aobject * path) {
    function_result result = { .has_return_value = true };
    
    // Convert AmLang string to C string (simplified)
    char* file_path = extract_string_from_aobject(path);
    
    // Implement file reading logic
    FILE* file = fopen(file_path, "r");
    if (file != NULL) {
        // Read file content and create AmLang string
        result.return_value.value.object_value = create_string_from_cstr("file content");
        result.return_value.flags = 0; // Object type
        fclose(file);
    } else {
        result.return_value.value.object_value = create_string_from_cstr("");
        result.return_value.flags = 0;
    }
    
    return result;
}

function_result Example_FileSystem_fileExists_0(aobject * const this, aobject * path) {
    function_result result = { .has_return_value = true };
    
    char* file_path = extract_string_from_aobject(path);
    struct stat buffer;
    result.return_value.value.bool_value = (stat(file_path, &buffer) == 0);
    result.return_value.flags = PRIMITIVE_NULL; // Primitive type
    
    return result;
}

// Required lifecycle functions for native classes
function_result Example_FileSystem__native_init_0(aobject * const this) {
    function_result result = { .has_return_value = false };
    // Initialize native resources if needed
    return result;
}

function_result Example_FileSystem__native_release_0(aobject * const this) {
    function_result result = { .has_return_value = false };
    // Release native resources if needed
    return result;
}

function_result Example_FileSystem__native_mark_children_0(aobject * const this) {
    function_result result = { .has_return_value = false };
    // Mark child objects for garbage collection if needed
    return result;
}
*/
```

### Collections
```amlang
import Am.Collections.List
import Am.Collections.HashMap
import Am.Collections.TreeSet

// Lists
var numbers = new List<Int>()
numbers.add(1)
numbers.add(2)
numbers.add(3)

// List methods
var size = numbers.getSize()  // Get size of list
var item = numbers.get(0)     // Get item at index
numbers.set(0, 10)           // Set item at index

// Array literals
var fruits = ["apple", "banana", "orange"]

// HashMaps
var scores = new HashMap<String, Int>()
scores.set("Alice", 95)
scores.set("Bob", 87)

// TreeSets (sorted)
var sortedNumbers = new TreeSet<Int>()
sortedNumbers.add(5)
sortedNumbers.add(1)
sortedNumbers.add(3)
```

### Control Flow
```amlang
// If statements
if (age >= 18) {
    "Adult".println()
} else {
    "Minor".println()
}

// Switch statements (check if 'default' is supported)
switch (grade) {
    case "A": "Excellent".println()
    case "B": "Good".println()
    case "C": "Average".println()
    // Note: Verify if 'default' case is supported in AmLang
}

// For loops - AmLang syntax
for (i = 0 to 10) {
    i.println()
}

// Enhanced for loops
for (item in collection) {
    item.println()
}

// While loops
while (condition) {
    // code
}

// Loop keyword (AmLang-specific infinite loop construct)
loop {
    // infinite loop - use break to exit
    if (condition) {
        break
    }
}
```

### String Operations
```amlang
import Am.Lang.String
import Am.Lang.StringBuilder

// String interpolation
var name = "World"
var message = "Hello, ${name}!"

// String methods
var text = "Hello World"
var length = text.getLength()
var upper = text.toUpper()
var contains = text.contains("World")

// StringBuilder for efficient concatenation
var builder = new StringBuilder()
builder.append("Hello")
builder.append(" ")
builder.append("World")
var result = builder.toString()
```

### Error Handling
```amlang
import Am.Lang.Exception

try {
    // risky operation
    var result = riskyFunction()
} catch (ex: Exception) {
    "Error: ${ex.getMessage()}".println()
}
```

### Generics
```amlang
// Generic class - only primary constructor available
class Container<T>(private var value: T) {
    fun getValue(): T {
        return this.value
    }
    
    fun setValue(newValue: T) {
        this.value = newValue
    }
    
    // Static factory functions for alternative construction
    static fun createEmpty<T>(): Container<T> {
        return new Container<T>(null)
    }
}

// Usage with 'new' keyword
var stringContainer = new Container<String>("Hello")
var intContainer = new Container<Int>(42)
var emptyContainer = Container.createEmpty<String>()
```

### Interfaces
```amlang
interface Drawable {
    fun draw()
    fun getArea(): Double
}

class Circle(private var radius: Double) : Drawable {
    override fun draw() {
        "Drawing circle".println()
    }
    
    override fun getArea(): Double {
        return 3.14159 * this.radius * this.radius
    }
}

// Usage with 'new' keyword
var circle = new Circle(5.0)
circle.draw()
```

## Testing Framework

AmLang includes a comprehensive testing framework with mocking capabilities:

```amlang
import Am.Tests.Assert

class CalculatorTest {
    fun testAddition() {
        var calc = new Calculator()
        var result = calc.add(2, 3)
        Assert.assertTrue(result == 5)
    }
    
    fun testWithMock() {
        mock Calculator {
            fun add(a: Int, b: Int): Int {
                return 100  // Mock implementation
            }
        }
        
        var calc = new Calculator()
        var result = calc.add(5, 10)
        Assert.assertTrue(result == 100)  // Uses mock
    }
}
```

### Native Function Mocking
```amlang
import Am.Tests.Assert

native class SystemUtils {
    native fun getCurrentTimeMillis(): Long
    native fun getRandomNumber(min: Int, max: Int): Int
}

class SystemTest {
    fun testTimeStamp() {
        mock SystemUtils {
            fun getCurrentTimeMillis(): Long {
                return 1234567890L  // Fixed timestamp for testing
            }
        }
        
        var utils = new SystemUtils()
        var time = utils.getCurrentTimeMillis()
        Assert.assertTrue(time == 1234567890L)
    }
}
```

## Primitive Types

- `Int`: 32-bit signed integer (literal: `42`)
- `Long`: 64-bit signed integer (literal: `42L`)
- `Short`: 16-bit signed integer (literal: `42S`)
- `Byte`: 8-bit signed integer (literal: `42B`)
- `UInt`: 32-bit unsigned integer (literal: `42U`)
- `ULong`: 64-bit unsigned integer (literal: `42UL`)
- `UShort`: 16-bit unsigned integer (literal: `42US`)
- `UByte`: 8-bit unsigned integer (literal: `42UB`)
- `Bool`: Boolean (true/false)
- `Double`: 64-bit floating point (literal: `3.14159D`)
- `Float`: 32-bit floating point (literal: `3.14F`)
- `String`: Text string (literal: `"text"`)
- `Char`: Single character (literal: `'c'`)

### Type-Specific Literal Examples
```amlang
var intValue: Int = 42
var longValue: Long = 1000L
var shortValue: Short = 5S
var byteValue: Byte = 127B
var uintValue: UInt = 42U
var ulongValue: ULong = 2000UL
var ushortValue: UShort = 10US
var ubyteValue: UByte = 255UB
var floatValue: Float = 3.14F
var doubleValue: Double = 3.14159D
var charValue: Char = 'A'
var stringValue: String = "Hello"
var boolValue: Bool = true
```

## Built-in Classes

### Core Classes
- `Object`: Base class for all objects
- `String`: Text manipulation
- `Array<T>`: Fixed-size arrays
- `StringBuilder`: Efficient string building

### Collections
- `List<T>`: Dynamic array
  - `add(item: T)`: Add item to list
  - `get(index: Int): T`: Get item at index
  - `set(index: Int, value: T)`: Set item at index
  - `getSize(): Int`: Get number of items
- `HashMap<K,V>`: Hash-based key-value storage
- `HashSet<T>`: Hash-based unique value storage
- `TreeSet<T>`: Sorted unique value storage
- `TreeMap<K,V>`: Sorted key-value storage

### I/O Classes
- `File`: File operations
- `FileStream`: File reading/writing
- `TextStream`: Text-based I/O
- `BinaryStream`: Binary I/O

### Utility Classes
- `Random`: Random number generation
- `Date`: Date and time operations
- `JsonParser`: JSON parsing
- `TextParser`: Text parsing utilities

## Common Patterns

### Singleton Pattern
```amlang
import Am.Lang.Object

class Logger {
    private static var instance: Logger? = null
    
    static fun getInstance(): Logger {
        if (Logger.instance == null) {
            Logger.instance = new Logger()
        }
        return Logger.instance!!
    }
    
    fun log(message: String) {
        "[LOG] ${message}".println()
    }
}

// Usage
var logger = Logger.getInstance()
```

### Factory Pattern
```amlang
interface Animal {
    fun makeSound()
}

class Dog : Animal {
    override fun makeSound() {
        "Woof!".println()
    }
}

class AnimalFactory {
    static fun createAnimal(type: String): Animal {
        switch (type) {
            case "dog": return new Dog()
            default: throw Exception("Unknown animal type")
        }
    }
}
```

### Observer Pattern
```amlang
interface Observer {
    fun update(message: String)
}

class Subject {
    private var observers = new List<Observer>()
    
    fun addObserver(observer: Observer) {
        this.observers.add(observer)
    }
    
    fun notifyObservers(message: String) {
        for (observer in this.observers) {
            observer.update(message)
        }
    }
}
```

## Package Structure

### package.yml Configuration
```yaml
id: "my-project"
version: "1.0.0"
type: "application"

dependencies:
  - id: am-lang-core
    realm: github
    type: git-repo
    tag: latest
    url: https://github.com/anderskjeldsen/am-lang-core.git
  - id: am-tests
    testOnly: true
    realm: github
    type: git-repo
    tag: latest
    url: https://github.com/anderskjeldsen/am-tests.git

platforms:
  - id: libc
    abstract: true
  - id: "linux-x64"
    extends: libc
    gccCommand: "gcc"
  - id: "linux-arm64"
    extends: libc
    gccCommand: "gcc"
  - id: "amigaos"
    gccCommand: "m68k-amigaos-gcc"
  - id: "morphos"
    gccCommand: "ppc-morphos-gcc"

buildTargets:
  - id: "linux-x64"
    platform: "linux-x64"
  - id: "linux-arm64"
    platform: "linux-arm64"
  - id: "linux-x64_docker"
    platform: "linux-x64"
    dockerBuild:
      image: "gcc:14.2.0"
      buildPath: "/usr/src/myapp"
  - id: "linux-arm64v8_docker"
    platform: "linux-arm64v8"
    dockerBuild:
      image: "arm64v8/gcc:14.2.0"
      buildPath: "/usr/src/myapp"
    dockerRun:
      image: "arm64v8/ubuntu:24.04"
      buildPath: "/usr/src/myapp"
  - id: "morphos-ppc_docker_win"
    platform: "morphos-ppc"
    commands:
      - "docker run --rm -v \"${build_dir}\":/work -e -i amigadev/crosstools:ppc-morphos make -f morphos-ppc.makefile -C /work all"
  - id: "aros-x86-64_docker_win"
    platform: "aros-x86-64"
    commands:
      - "docker run --rm -v \"${build_dir}\":/work -i amigadev/crosstools:x86_64-aros make aros-x86-64.makefile all"
  - id: "amigaos"
    platform: "amigaos"
    dockerBuild:
      image: "amiga-gcc"
      buildPath: "/host"
```

### Directory Structure
```
my-project/
├── package.yml
├── Makefile
├── src/
│   ├── am-lang/
│   │   └── MyProject/
│   │       ├── App.aml
│   │       ├── models/
│   │       │   └── User.aml
│   │       └── utils/
│   │           └── Helper.aml
│   └── native/
│       └── MyProject/
│           └── FileSystem.c
└── tests/
    └── AppTest.aml
```

## Build System

### Commands
- `amlc new`: Create new project (interactive - prompts for project name)
- `amlc build . -bt <target>`: Compile project for specific target (e.g., `amlc build . -bt linux-x64`)
- `amlc run . -bt <target>`: Run the compiled application for specific target (e.g., `amlc run . -bt linux-x64`)
- `amlc test . -bt <target> [-tests <test-pattern>]`: Run unit tests for specific target (optionally filter by test name pattern)
- `amlc clean`: Clean build artifacts

### Detailed Command Usage

#### Building Projects
```bash
# Build for specific target
amlc build . -bt linux-x64

# Build all configured targets
amlc build .
```

#### Running Applications
```bash
# Run for default target (first configured build target)
amlc run .

# Run for specific target
amlc run . -bt linux-x64

# Run with arguments
amlc run . -bt linux-x64 -- arg1 arg2
```

#### Running Tests
```bash
# Run all tests for specific target
amlc test . -bt linux-x64

# Run tests matching a pattern
amlc test . -bt linux-x64 -tests Calculator

# Run specific test class
amlc test . -bt linux-x64 -tests CalculatorTest

# Run tests on macOS
amlc test . -bt macos -tests TestPatternDemo
```

### Build Target Examples
```bash
# Build for Linux x64 (native)
amlc build . -bt linux-x64

# Build for Linux ARM64 (native on ARM64)
amlc build . -bt linux-arm64

# Build for Linux x64 using Docker
amlc build . -bt linux-x64_docker

# Build for Linux ARM64 using Docker (cross-compile)
amlc build . -bt linux-arm64v8_docker

# Build for MorphOS using Docker with custom commands
amlc build . -bt morphos-ppc_docker_win

# Build for AROS using Docker with custom commands
amlc build . -bt aros-x86-64_docker_win

# Build for AmigaOS using Docker
amlc build . -bt amigaos
```

### Cross-Platform Compilation
AmLang supports both native and cross-compilation depending on the build target:
- **linux-x64**: Linux x86-64 (native compilation on Linux x64)
- **linux-arm64**: Linux ARM64 (native compilation on ARM64 systems)  
- **linux-x64_docker**: Linux x64 using Docker with GCC 14.2.0
- **linux-arm64v8_docker**: Linux ARM64 using Docker (cross-compilation with separate run environment)
- **morphos-ppc_docker_win**: MorphOS PowerPC using Docker with custom commands
- **aros-x86-64_docker_win**: AROS x86-64 using Docker with custom commands
- **amigaos**: AmigaOS m68k using Docker (cross-compilation)

## Native Integration

### Defining Native Functions
```amlang
native class DatabaseConnection {
    native fun connect(connectionString: String): Bool
    native fun execute(query: String): ResultSet
    native fun close()
}
```

### Implementing in C
```c
// DatabaseConnection.c
function_result Example_DatabaseConnection_connect_0(aobject * const this, aobject * connectionString) {
    function_result result = { .has_return_value = true };
    // Implementation here
    result.return_value.value.bool_value = true;
    result.return_value.flags = PRIMITIVE_NULL;
    return result;
}
```

## Advanced Features

### Annotations
```amlang
@UseMemoryPool
class PerformanceCritical {
    // This class will use memory pooling
}
```

### Threading
```amlang
class Worker : Runnable {
    override fun run() {
        "Worker thread running".println()
    }
}

// Usage
var worker = new Worker()
var thread = new Thread(worker)
thread.start()
```

### Function References
```amlang
fun processData(data: String, processor: (String) -> String): String {
    return processor(data)
}

// Usage
var result = processData("hello", { s -> s.toUpper() })
```

## Best Practices

1. **Use descriptive names**: `calculateTotalPrice()` instead of `calc()`
2. **Prefer immutability**: Use `const val` when possible
3. **Handle errors gracefully**: Always use try-catch for risky operations
4. **Write tests**: Every public function should have corresponding tests
5. **Use native classes wisely**: Only for system integration or performance-critical code
6. **Leverage collections**: Use appropriate collection types for your data
7. **Follow naming conventions**: 
   - Classes: `PascalCase`
   - Functions/variables: `camelCase`
   - Constants: `UPPER_SNAKE_CASE`

## Common Gotchas

1. **Null safety**: AmLang is null-safe by default. Use `?` for nullable types
2. **String immutability**: Strings are immutable; use StringBuilder for concatenation
3. **Native function signatures**: Must match exactly between AmLang and C implementations
4. **Mock limitations**: Mocks only work within the scope they're declared
5. **Collection generics**: Always specify generic types for type safety

## IDE Support

AmLang works well with:
- **VS Code**: Syntax highlighting and basic completion
- **IntelliJ IDEA**: Advanced features with plugin
- **Vim/Neovim**: Syntax highlighting available

## Debugging

### Compile-time debugging
```amlang
#if DEBUG
"Debug mode active".println()
#endif
```

### Runtime debugging
```amlang
import Am.Lang.Diagnostics.Debug

Debug.print("Variable value: ${myVariable}")
Debug.assert(condition, "Assertion message")
```

This guide should help AI assistants understand AmLang's capabilities and provide accurate code suggestions. For the most up-to-date information, refer to the official AmLang documentation.