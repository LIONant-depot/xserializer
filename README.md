# xserializer: Supercharge Your C++ Resource Serialization!

Transform your data handling with **xserializer** – a high-performance, memory-efficient C++ library designed for blazing-fast 
binary serialization. Say goodbye to complex data marshalling and hello to a streamlined, user-friendly interface that saves 
and loads data in its native machine format. With zero-copy loading, smart memory management, and flexible serialization options, 
xserializer is your ultimate tool for resource-heavy applications!

## Key Features

- **Superfast loading**: Loads while at the same time decompresses in place
- **zero-copy loading**: Serializes the data structures that will be used at runtime 
- **Memory Efficient**: Groups all possible allocations into a single one.
- **Automatic pointer handling**: The file format will automatically resolve all the pointers at load time.
- **Handles 32/64 bits**: If you use ```data_ptr<>``` for your pointers your file will work for both platforms.
- **Handles big/small endian**: While saving the file it will convert automatically to the right endian.
- **Flexible Serialization**: Use `Serialize` for atomic types, arrays, and pointers, and `SerializeEnum` for enums.
- **Smart Memory Management**: Supports different types of memory options like VRam, temp memory and regular memory for optimized resource handling.
- **Three-Stage Loading**: Header loading, object loading, and resolution for seamless integration with game systems like VRAM or animation managers.
- **Compression Support**: Automatically compresses all the data using different compression levels (`FAST`, `LOW`, `MEDIUM`, `HIGH`).
- **MIT License**: Free and open for all your projects.
- **Minimal Dependencies**: Requires only `xfile` for file I/O and `xerr` for error handling.
- **Easy Integration**: Just include `xserializer.h` and `xserializer.cpp` in your project.
- **Unit test & Documentation**: Check the documentation here

## Dependencies

- [xfile](https://github.com/LIONant-depot/xfile): For high-performance asynchronous file I/O operations.
- [xerr](https://github.com/LIONant-depot/xerr): For fast and robust error handling.
- [xcompression](https://github.com/LIONant-depot/xcompression): For fast compression and decompression of data.

## Getting started

- Run the ```build\updateDependencies.bat``` to collect all the dependendencies
- Simply add ```xserializer.h``` and ```xserializer.cpp``` in your project
- You will need to add the paths for any dependency (include for xcompression for the lib)


## Code Example

```cpp
#include "xserializer.h"

struct MyStruct {
    int     value;
    int     count;
    float*  pdata;
};

// Define SerializeIO for custom serialization
namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const MyStruct& S) noexcept {
        Stream.Serialize(S.value);
        Stream.Serialize(S.count);
        Stream.Serialize(S.pdata, S.count, { .m_bUnique = true });
        return {};
    }
}

int main() {

    // save
    {
        xserializer::stream serializer;
        float* p = new float[3]; p[0] = 1.0f;  p[1] = 1.0f; p[2] = 1.0f;
        MyStruct obj{ 42, 3, p };

        // Save to file
        if (auto err = serializer.Save(L"temp:\\data.bin", obj, xserializer::compression_level::MEDIUM); err) {
            // Handle error
            return 1;
        }

        delete p;
    }

    // Load from file
    {
        xserializer::stream serializer;

        MyStruct* loadedObj = nullptr;
        if (auto err = serializer.Load(L"temp:\\data.bin", loadedObj); err) {
            // Handle error
            return 1;
        }

        // Now is fully ready!

        // xserializer takes ownership of pdata so not need to free that
        // When we are done, we can free the loaded object’s memory
        xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true}, loadedObj );
    }

    return 0;
}
```

## Physical File Layout on Disk

The file layout on disk is optimized for efficient loading:

```plaintext
+----------------+
| File Header    | <- File header, never allocated (1st load)
+----------------+
| BlockSizes     |
| PointerInfo    | <- Temporary data, deleted after LoadObject (2nd load)
| PackInfo       |
+----------------+
| Blocks         | <- Compressed data blocks with user data (3rd load)
+----------------+
```

Dive into xserializer today – star, fork, and contribute to revolutionize your data serialization! 🚀
