# Frequently Asked Questions (FAQ)

This FAQ answers common questions students might have when using `xserializer`.

## Why do I get a “namespace not found” error?

Ensure your `SerializeIO` function is in the `xserializer::io_functions` namespace:
```cpp
namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const MyStruct& S) noexcept {
        // ...
    }
}
```

## Why does my program crash when loading?

- **Check Memory**: Ensure you free `m_bUnique` memory with `default_memory_handler_v.Free`.
- **Verify Versions**: Set and check `setResourceVersion` to match your data version.
- **Debug with Sanity Checks**: Add checks like in the unit test to verify loaded data.

## How do I handle dynamic arrays?

Use `Stream.Serialize(pointer, size, mem_type)` and specify the array size. Example:
```cpp
Stream.Serialize(myArray, count, { .m_bUnique = true });
```

## What’s the difference between `m_bUnique` and `m_bTempMemory`?

- `m_bUnique`: Memory you must free manually.
- `m_bTempMemory`: Temporary memory freed automatically unless you call `DontFreeTempData()`.

## Why use `data_ptr` instead of `T*`?

`data_ptr<T>` ensures your pointers work on both 32-bit and 64-bit systems, making your code portable.

## How do I enable compression?

Set the compression level when saving:
```cpp
serializer.Save(L"data.bin", myData, xserializer::compression_level::MEDIUM);
```

## Where can I find more examples?

Check [Example Walkthrough](ExampleWalkthrough.md) for a detailed example based on the unit tests.

If you have more questions, check the [GitHub repository](https://github.com/LIONant-depot/xserializer) or ask in the community!