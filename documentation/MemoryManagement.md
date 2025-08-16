# Memory Management in xserializer

`xserializer` is designed to be memory-efficient, grouping allocations into a single block where possible. This guide explains how to use `data_ptr`, memory types (`m_bUnique`, `m_bTempMemory`, `m_bVRAM`), and how to manage memory safely.

## The `data_ptr` Template

`data_ptr<T>` is a special pointer type that ensures compatibility across 32-bit and 64-bit systems. Use it instead of raw pointers (`T*`) in your structs to make your data portable.

Example:
```cpp
struct MyData {
    xserializer::data_ptr<int> numbers; // Portable pointer
};
```

## Memory Types

When serializing pointers or arrays, you can specify a `mem_type` to control how memory is handled:

- **m_bUnique**: Memory is allocated separately and must be freed manually. Use for data that lives independently.
- **m_bTempMemory**: Temporary memory that’s freed after loading unless you take ownership. Useful for short-lived data.
- **m_bVRAM**: Marks memory for VRAM (video RAM) on systems with graphics hardware. Not used in most student projects.

Example:
```cpp
float* data = new float[5];
Stream.Serialize(data, 5, { .m_bUnique = true }); // Unique memory, must free
```

## Freeing Memory

After loading, you may need to free memory for `m_bUnique` data using `default_memory_handler_v.Free`:

```cpp
MyStruct* loadedObj;
serializer.Load(L"data.bin", loadedObj);
// Use loadedObj
xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true }, loadedObj);
```

For `m_bTempMemory`, `xserializer` frees it automatically unless you call `DontFreeTempData()` and take ownership.

## Example with Memory Management

```cpp
struct GameObject {
    int id;
    xserializer::data_ptr<float> positions;
};

namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const GameObject& Obj) noexcept {
        Stream.Serialize(Obj.id);
        Stream.Serialize(Obj.positions.m_pValue, 3, { .m_bUnique = true });
        return {};
    }
}

int main() {
    xserializer::stream serializer;
    GameObject obj{ 1, { new float[3]{ 0.0f, 1.0f, 2.0f } } };
    serializer.Save(L"game.bin", obj);
    GameObject* loadedObj;
    serializer.Load(L"game.bin", loadedObj);
    xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true }, loadedObj);
    return 0;
}
```

## Tips for Students

- **Use `data_ptr`**: It simplifies cross-platform pointer handling.
- **Free Unique Memory**: Always free `m_bUnique` memory to avoid leaks.
- **Avoid VRAM for Now**: Unless you’re working on graphics, stick to `m_bUnique` or `m_bTempMemory`.

Next, explore the loading process in [Three-Stage Loading](ThreeStageLoading.md).