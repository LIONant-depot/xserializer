# Serialization Basics in xserializer

Serialization is like packing your data into a box to store it and unpacking it later exactly as it was. This guide explains how to use `xserializer` to save and load data, focusing on the `Serialize` and `SerializeEnum` functions and how to define `SerializeIO` for your custom structs.

## The `Serialize` Function

The `Serialize` function is the core of `xserializer`. It saves or loads basic data types (like `int`, `float`, or arrays) and pointers. Here’s how it works:

- **For Basic Types**: Use `Stream.Serialize(myVariable)` to save/load numbers, strings, etc.
- **For Arrays/Pointers**: Use `Stream.Serialize(myPointer, size, memoryType)` to handle dynamic data.
- **Return Value**: Returns an `xerr` object to check for errors (empty `{}` means success).

Example:
```cpp
int x = 42;
Stream.Serialize(x); // Saves or loads x
float* arr = new float[3]{ 1.0f, 2.0f, 3.0f };
Stream.Serialize(arr, 3, { .m_bUnique = true }); // Saves/loads array with unique memory
```

## The `SerializeEnum` Function

For enums, use `SerializeEnum` to ensure proper handling, especially across platforms with different endianness (how bytes are ordered). Example:

```cpp
enum class Color { RED, GREEN, BLUE };
Stream.SerializeEnum(Color::GREEN);
```

## Defining `SerializeIO` for Custom Structs

To serialize your own structs, define a `SerializeIO` function in the `xserializer::io_functions` namespace. This tells `xserializer` how to handle your struct’s fields.

### Example

```cpp
struct Person {
    int age;
    std::string name;
};

namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const Person& P) noexcept {
        Stream.Serialize(P.age);
        Stream.Serialize(P.name.data(), P.name.size());
        return {};
    }
}
```

- **Steps**:
  1. Use `Stream.Serialize` for each field.
  2. Return `{}` if successful, or an `xerr` object if there’s an error.
  3. Place the function in `xserializer::io_functions`.

### Why `noexcept`?

The `noexcept` keyword ensures the function doesn’t throw exceptions, which is important for performance and reliability in `xserializer`.

## Tips for Students

- **Start Simple**: Begin with basic types (`int`, `float`) before tackling pointers or arrays.
- **Check Errors**: Always check the `xerr` return value with `if (err) { /* handle error */ }`.
- **Use Namespaces**: Ensure `SerializeIO` is in the correct namespace, or it won’t work!

Next, learn how to manage memory for pointers and arrays in [Memory Management](MemoryManagement.md).