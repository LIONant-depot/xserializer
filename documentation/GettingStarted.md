# Getting Started with xserializer

This guide will help you set up **xserializer** and run your first program. It’s designed for students new to C++ or serialization, so we’ll keep it simple and clear.

## Prerequisites

- A C++ compiler (e.g., GCC, Clang, MSVC) with C++20 support.
- The `xserializer` library files: `xserializer.h` and `xserializer.cpp`.
- Dependencies: `xfile` (for file I/O) and `xerr` (for error handling). Optionally, `xcompression` for compression support.
- A basic understanding of C++ structs, pointers, and namespaces.

## Setting Up

1. **Download the Library**:
   - Clone or download the `xserializer` repository from [GitHub](https://github.com/LIONant-depot/xserializer).
   - Ensure you have `xfile` and `xerr` from their respective repositories ([xfile](https://github.com/LIONant-depot/xfile), [xerr](https://github.com/LIONant-depot/xerr)).

2. **Add to Your Project**:
   - Copy `xserializer.h` and `xserializer.cpp` into your project directory.
   - Include `xserializer.h` in your code with `#include "xserializer.h"`.

3. **Build Your Project**:
   - Compile your project with a C++20-compliant compiler. For example:
     ```bash
     g++ -std=c++20 main.cpp xserializer.cpp -o myprogram
     ```

## Your First Program

Let’s create a simple program that saves and loads a struct with `xserializer`.

### Example Code

```cpp
#include "xserializer.h"

struct Student {
    int id;
    float grade;
};

// Tell xserializer how to serialize the Student struct
namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const Student& S) noexcept {
        Stream.Serialize(S.id);
        Stream.Serialize(S.grade);
        return {};
    }
}

int main() {
    // Save
    {
        xserializer::stream serializer;
        Student s{ 123, 95.5f };
        if (auto err = serializer.Save(L"temp:\\student.bin", s); err) {
            // Handle error
            return 1;
        }
    }
    // Load
    {
        xserializer::stream serializer;
        Student* loadedStudent = nullptr;
        if (auto err = serializer.Load(L"temp:\\student.bin", loadedStudent); err) {
            // Handle error
            return 1;
        }
        // Use loadedStudent (e.g., print loadedStudent->id)
        xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true }, loadedStudent);
    }
    return 0;
}
```

### What’s Happening?

1. **Define a Struct**: We created a `Student` struct with an ID and grade.
2. **SerializeIO Function**: We told `xserializer` how to save/load the struct by defining `SerializeIO` in the `xserializer::io_functions` namespace.
3. **Save**: We created a `Student` object, saved it to a file (`temp:\\student.bin`), and checked for errors.
4. **Load**: We loaded the data back into a `Student*`, used it, and freed the memory.

### Next Steps

- Learn how to serialize more complex data (like arrays and pointers) in [Serialization Basics](SerializationBasics.md).
- Explore memory management in [Memory Management](MemoryManagement.md).