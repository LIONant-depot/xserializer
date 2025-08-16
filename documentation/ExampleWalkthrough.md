# Example Walkthrough: Serializing Complex Data

This guide walks through the unit test example from `xserializer` to show how to serialize complex data with pointers, arrays, and custom memory types. It’s based on the unit test code, broken down for students.

## The Data Structures

We have three structs: `data1`, `data2`, and `data3`.

- **data1**: A simple struct with a 16-bit integer (`m_A`).
- **data2**: Contains a count and a `data_ptr<data1>` for an array of `data1`.
- **data3**: Inherits from `data1`, with a static `data2`, a dynamic `data2`, and an array of `data2` for temporary memory.

```cpp
struct data1 {
    std::int16_t m_A;
};
struct data2 {
    std::size_t m_Count;
    xserializer::data_ptr<data1> m_Data;
};
struct data3 : public data1 {
    data2 m_GoInStatic;
    data2 m_DontDynamic;
    std::array<data2, 8> m_GoTemp;
    // ... constructor, destructor, etc.
};
```

## Serializing the Data

Each struct needs a `SerializeIO` function:

```cpp
namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const data1& Data) noexcept {
        Stream.Serialize(Data.m_A);
        return {};
    }
    xerr SerializeIO(xserializer::stream& Stream, const data2& Data) noexcept {
        Stream.Serialize(Data.m_Count);
        Stream.Serialize(Data.m_Data.m_pValue, Data.m_Count);
        return {};
    }
    xerr SerializeIO(xserializer::stream& Stream, const data3& Data) noexcept {
        Stream.setResourceVersion(1);
        Stream.Serialize(Data.m_GoInStatic);
        Stream.Serialize(Data.m_DontDynamic.m_Count);
        Stream.Serialize(Data.m_DontDynamic.m_Data.m_pValue, Data.m_DontDynamic.m_Count, { .m_bUnique = true });
        for (auto& Temp : Data.m_GoTemp) {
            Stream.Serialize(Temp.m_Count);
            Stream.Serialize(Temp.m_Data.m_pValue, Temp.m_Count, { .m_bTempMemory = true });
        }
        SerializeIO(Stream, static_cast<const data1&>(Data));
        return {};
    }
}
```

- **data1**: Serializes a single integer.
- **data2**: Serializes a count and an array of `data1`.
- **data3**: Serializes a static `data2`, a dynamic `data2` with unique memory, an array of temporary `data2`, and the parent `data1`.

## Saving and Loading

The unit test saves and loads a `data3` object:

```cpp
void Test01() {
    std::wstring_view FileName(L"temp:/SerialFile.bin");
    // Save
    {
        xserializer::stream SerialFile;
        data3 TheData;
        TheData.SanityCheck();
        SerialFile.Save(FileName, TheData);
        TheData.DestroyStaticStuff();
    }
    // Load
    {
        xserializer::stream SerialFile;
        data3* pTheData;
        SerialFile.Load(FileName, pTheData);
        pTheData->SanityCheck();
        xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true }, pTheData);
    }
}
```

### Steps Explained

1. **Saving**:
   - Creates a `data3` object with initialized data.
   - Calls `SanityCheck` to verify data.
   - Saves to `temp:/SerialFile.bin` using `Save`.
   - Calls `DestroyStaticStuff` to free static memory.

2. **Loading**:
   - Loads the file into a `data3*` pointer using `Load`.
   - Runs `SanityCheck` to verify the loaded data.
   - Frees the unique memory with `default_memory_handler_v.Free`.

## Key Lessons

- **Custom Constructors**: `data3` has a special constructor for resolution to handle dynamic data.
- **Memory Types**: Uses `m_bUnique` for dynamic data and `m_bTempMemory` for temporary arrays.
- **Sanity Checks**: Verifies data integrity after loading.

## Tips for Students

- **Break It Down**: Start with simple structs before adding pointers or arrays.
- **Test Your Data**: Use sanity checks to ensure your data loads correctly.
- **Free Memory**: Always free `m_bUnique` memory to avoid leaks.

Check [FAQ](FAQ.md) for common issues and solutions.