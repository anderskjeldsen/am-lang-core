# AmLang Core Standard Library (am-lang-core)

This repository contains the core standard library for the AmLang programming language ecosystem. AmLang is a modern object-oriented programming language designed for systems programming with cross-platform compatibility, particularly targeting niche platforms like AmigaOS, MorphOS, Raspberry Pi Pico, and legacy systems.

## Repository Overview

**am-lang-core** is the foundational library that provides essential classes and functionality required by every AmLang program. It serves as the standard library containing core data types, collections, I/O operations, networking primitives, and memory management features designed to work across all AmLang-supported platforms.

### Key Features

- **Core Data Types**: `Int`, `String`, `Bool`, `Double`, `Long`, and primitive type wrappers
- **Collections Framework**: `List<T>`, `HashMap<K,V>`, `HashSet<T>`, `TreeSet<T>`, `TreeMap<K,V>`
- **I/O System**: `File`, `FileStream`, `TextStream`, `BinaryStream` for file and stream operations
- **Networking**: `Socket` class for TCP/UDP networking (basic implementation)
- **Memory Management**: Automatic Reference Counting (ARC) with native C implementation
- **Text Processing**: `StringBuilder`, `TextParser` for efficient string manipulation
- **Date/Time**: `DateTime` class for temporal operations
- **Exception Handling**: Complete exception hierarchy and stack trace support
- **Cross-Platform**: Native implementations for AmigaOS, Linux, macOS, MorphOS, AROS

### Core Classes

- **Language Primitives**: `Int`, `String`, `Bool`, `Double`, `Long`, `UInt`, `UShort`, etc.
- **Collections**: `List<T>`, `HashMap<K,V>`, `HashSet<T>`, `TreeSet<T>`, `Array<T>`
- **I/O Classes**: `File`, `FileStream`, `TextStream`, `BinaryStream`, `Stream`
- **Networking**: `Socket`, `AddressFamily`, `SocketType`, `ProtocolFamily`
- **Utilities**: `StringBuilder`, `TextParser`, `DateTime`, `Random`
- **Exception System**: `Exception`, `IndexOutOfBoundsException`, `NullReferenceException`

## API Usage Patterns

### Basic Data Types and String Operations

```amlang
import Am.Lang

// String operations
var message = "Hello, AmLang!"
var length = message.length()
var upper = message.toUpperCase()
var substring = message.substring(0UI, 5UI)
var substringToEnd = message.substringToEnd(7UI)

// String interpolation
var name = "World"
var greeting = "Hello, ${name}!"

// String manipulation with new methods
var text = "apple,banana,cherry"
var parts = text.splitByString(",")  // Returns Array<String>
var joined = String.join(parts, " | ")  // "apple | banana | cherry"
var afterComma = text.indexOfAfter(",")  // Find index after first comma

// StringBuilder for efficient string building
var sb = new StringBuilder()
sb.append("Line 1")
sb.appendLine()
sb.append("Line 2")
var result = sb.toString()
```

### Collections Framework

```amlang
import Am.Collections
import Am.Lang

// Lists - dynamic arrays
var numbers = List.newList<Int>()
numbers.add(1)
numbers.add(2)
numbers.add(3)
var size = numbers.getSize()
var first = numbers.get(0)

// HashMaps - key-value storage
var scores = HashMap.newHashMap<String, Int>()
scores.set("Alice", 95)
scores.set("Bob", 87)
var aliceScore = scores.get("Alice")
var containsBob = scores.containsKey("Bob")

// HashSets - unique values
var uniqueNames = HashSet.newHashSet<String>()
uniqueNames.add("Alice")
uniqueNames.add("Bob")
uniqueNames.add("Alice")  // Duplicate ignored
var hasAlice = uniqueNames.contains("Alice")

// TreeSets - sorted unique values
var sortedNumbers = TreeSet.newTreeSet<Int>()
sortedNumbers.add(5)
sortedNumbers.add(1)
sortedNumbers.add(3)
// Iteration will be in sorted order: 1, 3, 5
```

### File I/O Operations

```amlang
import Am.IO
import Am.Lang

// Reading text files
var file = new File("data.txt")
if (file.exists()) {
    var content = String.readFromFile("data.txt")
    content.println()
}

// Writing to files
var outputFile = new File("output.txt")
var fileStream = new FileStream(outputFile, FileAccess.writeOnly)
var textStream = new TextStream(fileStream)
textStream.writeLine("Hello, file!")
textStream.writeLine("Second line")
fileStream.close()

// Binary file operations
var binaryFile = new File("data.bin")
var binaryStream = new FileStream(binaryFile, FileAccess.readOnly)
var bytes = binaryStream.readAll()
binaryStream.close()
```

### Exception Handling

```amlang
import Am.Lang

try {
    var list = List.newList<String>()
    var item = list.get(5)  // Will throw IndexOutOfBoundsException
} catch (e: IndexOutOfBoundsException) {
    "Index error: ${e.message}".println()
} catch (e: Exception) {
    "General error: ${e.message}".println()
    e.printWithStackTrace()
}
```

### Basic Networking

```amlang
import Am.IO.Networking
import Am.Lang

// TCP client socket
var socket = Socket.create(AddressFamily.inet, SocketType.stream, ProtocolFamily.tcp)
socket.connect("example.com", 80)

// Send HTTP request
var request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
var requestBytes = request.getBytes("UTF-8")
socket.send(requestBytes, requestBytes.length())

// Receive response
var buffer: UByte[] = new UByte[1024]
var received = socket.receive(buffer, 1024UI)

socket.close()
```

## Project Structure

```
src/am-lang/Am/
├── Lang/                    # Core language types
│   ├── Object.aml          # Base object class
│   ├── String.aml          # String manipulation
│   ├── Int.aml             # Integer types
│   ├── Bool.aml            # Boolean type
│   ├── Exception.aml       # Exception hierarchy
│   ├── StringBuilder.aml   # Efficient string building
│   └── DateTime.aml        # Date/time operations
├── Collections/            # Collection classes
│   ├── List.aml           # Dynamic arrays
│   ├── HashMap.aml        # Hash-based key-value storage
│   ├── HashSet.aml        # Hash-based unique value storage
│   ├── TreeSet.aml        # Sorted unique value storage
│   └── TreeMap.aml        # Sorted key-value storage
├── IO/                    # Input/Output operations
│   ├── File.aml           # File operations
│   ├── FileStream.aml     # File stream I/O
│   ├── TextStream.aml     # Text-based I/O
│   ├── BinaryStream.aml   # Binary I/O
│   └── Networking/        # Basic networking
│       ├── Socket.aml     # TCP/UDP sockets
│       ├── AddressFamily.aml
│       ├── SocketType.aml
│       └── ProtocolFamily.aml
├── Util/                  # Utility classes
│   ├── Parsers/Text/
│   │   └── TextParser.aml # Text parsing utilities
│   └── Random.aml         # Random number generation
└── Tests/                 # Testing framework
    └── Assert.aml         # Unit testing assertions

src/native-c/              # Platform-specific native implementations
├── libc/                  # Common POSIX implementation
├── linux-x64/             # Linux x64 optimizations
├── macos/                 # macOS implementations
├── macos-arm/             # Apple Silicon optimizations
├── amigaos/               # AmigaOS 68k implementations
├── morphos-ppc/           # MorphOS PowerPC implementations
└── aros-x86-64/           # AROS x86-64 implementations

tests/                     # Test suite
├── StringTests.aml        # String manipulation tests
├── CollectionsTests.aml   # Collection framework tests
├── FileIOTests.aml        # File I/O tests
└── NetworkingTests.aml    # Basic networking tests
```

## Building and Testing

### Prerequisites

- AmLang compiler (`amlc`) v0.6.4 or later
- Platform-specific C compilers (GCC, Clang, m68k-amigaos-gcc, etc.)
- Docker for cross-platform builds

### Build Commands

```bash
# Build for current platform
make build

# Run all tests
make test

# Run tests with detailed reference counting output
make test-rl

# Debug tests interactively
make gdb-test-interactive

# Clean build artifacts
make clean

# Show build status
make status
```

### Cross-Platform Builds

```bash
# Build for AmigaOS (requires Docker)
make build BT=amigaos_docker_unix-x64

# Build for MorphOS (requires Docker)
make build BT=morphos-ppc_docker

# Build for AROS
make build BT=aros-x86-64

# Build for Linux ARM64
make build BT=linux-arm64v8

# Build for macOS Apple Silicon
make build BT=macos-arm
```

### Testing and Debugging

```bash
# Run specific test categories
make test TEST=StringTests
make test TEST=CollectionsTests

# Debug with reference counting logs
make test-rl

# Interactive debugging
make gdb-test-interactive

# Check for memory leaks
make test-rl | grep "LEAK\|reference count"
```

## Development Guidelines

### Critical API Patterns

1. **String Method Usage**:
   ```amlang
   // CORRECT: Use new string methods
   var parts = text.splitByString(",")
   var joined = String.join(parts, " | ")
   var afterIndex = text.indexOfAfter(":")
   var toEnd = text.substringToEnd(5UI)
   
   // WRONG: Don't use non-existent methods
   var parts = text.split(",")  // split() doesn't exist, use splitByString()
   ```

2. **Collection Initialization**:
   ```amlang
   // CORRECT: Use static factory methods
   var list = List.newList<String>()
   var map = HashMap.newHashMap<String, Int>()
   
   // WRONG: Don't use direct constructors
   var list = new List<String>()  // Constructor not available
   ```

3. **File Operations**:
   ```amlang
   // CORRECT: Proper resource management
   var fileStream = new FileStream(file, FileAccess.readOnly)
   try {
       // Use stream...
   } finally {
       fileStream.close()
   }
   ```

4. **Exception Handling**:
   ```amlang
   // CORRECT: Specific exception types
   try {
       var item = list.get(index)
   } catch (e: IndexOutOfBoundsException) {
       // Handle index errors specifically
   } catch (e: Exception) {
       // Handle general errors
   }
   ```

### Memory Management Best Practices

- **Reference Counting**: AmLang uses ARC (Automatic Reference Counting)
- **Circular References**: Avoid circular object references
- **Native Resources**: Always close streams, files, and sockets
- **Large Objects**: Be mindful of memory usage on embedded platforms

### Cross-Platform Considerations

- **AmigaOS 68k**: Limited memory, optimized for 16-bit operations
- **MorphOS PowerPC**: Good performance, some modern features
- **Linux/macOS**: Full feature support, development platforms
- **Embedded Platforms**: Resource constraints, careful memory management

## Platform Support

| Platform | Status | Memory Model | I/O Support | Networking |
|----------|---------|-------------|-------------|------------|
| Linux x64 | ✅ Full | 64-bit ARC | Full | TCP/UDP |
| Linux ARM64 | ✅ Full | 64-bit ARC | Full | TCP/UDP |
| macOS x64 | ✅ Full | 64-bit ARC | Full | TCP/UDP |
| macOS ARM64 | ✅ Full | 64-bit ARC | Full | TCP/UDP |
| AmigaOS 68k | ✅ Full | 32-bit ARC | Full | TCP/UDP |
| MorphOS PPC | ✅ Full | 32-bit ARC | Full | TCP/UDP |
| AROS x86-64 | ✅ Full | 64-bit ARC | Full | Limited |
| Windows x64 | 🚧 Partial | 64-bit ARC | Partial | None |

## Core Classes Reference

### String Class

```amlang
class String {
    // Length and basic operations
    fun length(): Int
    fun getLength(): Int
    fun print()
    fun println()
    
    // String manipulation
    fun substring(start: UInt, length: UInt): String
    fun substringToEnd(start: UInt): String
    fun indexOf(searchString: String): Int
    fun indexOfAfter(searchString: String): Int
    fun lastIndexOf(searchString: String): String
    
    // Array operations
    fun splitByString(delimiter: String): String[]
    static fun join(parts: String[], delimiter: String): String
    
    // Case operations
    fun toUpperCase(): String
    fun toLowerCase(): String
    
    // Conversions
    fun getBytes(encoding: String): UByte[]
    static fun fromBytes(bytes: UByte[], encoding: String): String
    static fun readFromFile(filePath: String): String
    
    // Operators
    op + (other: String): String
    override fun equals(other: Object): Bool
    override fun hash(): UInt
}
```

### List Class

```amlang
class List<T> : Iterable<T> {
    static fun newList<T>(): List<T>
    
    fun add(item: T)
    fun addArray(items: T[])
    fun get(index: Int): T
    fun set(index: Int, item: T)
    fun remove(item: T): Bool
    fun removeAt(index: Int): T
    fun getSize(): Int
    fun contains(item: T): Bool
    fun clear()
    fun toArray(): T[]
    
    // Iteration support
    fun iterator(): Iterator<T>
}
```

### HashMap Class

```amlang
class HashMap<K, V> {
    static fun newHashMap<K, V>(): HashMap<K, V>
    
    fun set(key: K, value: V)
    fun get(key: K): V
    fun remove(key: K): Bool
    fun containsKey(key: K): Bool
    fun getKeys(): K[]
    fun getValues(): V[]
    fun getSize(): Int
    fun clear()
}
```

### File I/O Classes

```amlang
class File(filePath: String) {
    fun exists(): Bool
    fun isDirectory(): Bool
    fun isFile(): Bool
    fun getSize(): Long
    fun delete(): Bool
    fun createDirectory(): Bool
    fun listFiles(): File[]
}

class FileStream(file: File, access: FileAccess) : Stream {
    fun read(buffer: UByte[], offset: Long, length: UInt): UInt
    fun write(buffer: UByte[], offset: Long, length: UInt)
    fun seekFromStart(offset: Long)
    fun close()
    fun readAll(): UByte[]
}

class TextStream(stream: Stream) {
    fun readLine(): String
    fun writeLine(text: String)
    fun writeString(text: String)
    fun readAll(): String
}
```

## Common Issues and Solutions

### String Splitting Not Working
**Problem**: `text.split(",")` method not found
**Solution**: Use `text.splitByString(",")` instead - function overloading not fully supported

### Collection Constructor Errors
**Problem**: `new List<String>()` constructor not available
**Solution**: Use static factory methods: `List.newList<String>()`

### Memory Leaks in Native Code
**Problem**: Reference count errors in native implementations
**Solution**: Run tests with `make test-rl` to debug reference counting

### Cross-Platform Build Issues
**Problem**: Native code doesn't compile on target platform
**Solution**: Use Docker builds for exotic platforms, check platform-specific headers

### String Method Missing
**Problem**: String operations not available
**Solution**: Check if method exists in String.aml, use alternative approaches like StringBuilder

## Testing Framework

### Unit Testing with Assert

```amlang
import Am.Tests
import Am.Lang

test myTest() {
    var result = calculateSomething()
    Assert.assertEquals(42, result)
    Assert.assertTrue(result > 0)
    Assert.assertFalse(result < 0)
    Assert.assertNotNull(result)
}
```

### Running Specific Tests

```bash
# Run all tests
make test

# Run with reference counting debug
make test-rl

# Filter test output
make test | grep PASS
make test | grep FAIL
```

## Contributing Guidelines

1. **Cross-Platform Compatibility**: Test on multiple platforms, especially AmigaOS
2. **Memory Management**: Ensure proper reference counting in native code
3. **API Consistency**: Follow existing patterns for method naming and signatures
4. **Performance**: Consider embedded platform limitations
5. **Testing**: Add comprehensive tests for new functionality
6. **Documentation**: Update AI assistant guides for API changes

## Version Compatibility

- **AmLang Compiler**: Requires v0.6.4+ for full feature support
- **Backwards Compatibility**: API stability maintained within major versions
- **Native ABI**: Binary compatibility across minor versions
- **Platform Support**: Long-term support for legacy platforms

---

This library is the foundation of the AmLang ecosystem. For broader context and cross-repository development, see the main AmLang workspace documentation.