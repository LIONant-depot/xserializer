# Advanced Usage of xserializer

This guide covers advanced features of `xserializer`, including compression, endian handling, and 32/64-bit compatibility. These are useful for students working on complex projects like games or cross-platform applications.

## Compression Support

`xserializer` can compress data to save disk space using four levels: `FAST`, `LOW`, `MEDIUM`, and `HIGH`. Higher levels save more space but take longer.

Example:
```cpp
serializer.Save(L"data.bin", myData, xserializer::compression_level::HIGH);
```

- **When to Use**: Use `FAST` for quick saves, `HIGH` for smaller files.
- **How It Works**: Data is compressed into blocks during saving and decompressed during loading.

## Endian Handling

Computers store numbers in different byte orders (big-endian or little-endian). `xserializer` automatically handles this with `setSwapEndian` and `SwapEndian`.

Example:
```cpp
serializer.setSwapEndian(true); // Force endian swap if needed
if (serializer.SwapEndian()) {
    // Handle endian-specific logic
}
```

- **Tip**: Use `Serialize` and `SerializeEnum` to let `xserializer` handle endianness automatically.

## 32/64-Bit Compatibility

`xserializer` supports both 32-bit and 64-bit systems using `data_ptr<T>`. This ensures pointers work across platforms.

Example:
```cpp
struct Data {
    xserializer::data_ptr<int> values;
};
```

- **How It Works**: The `X_EXPTR` macro (mentioned in the header) ensures pointer sizes are handled correctly.
- **Tip**: Always use `data_ptr` for pointers in your structs.

## Versioning

Set a version for your data to handle future changes:
```cpp
struct MyData {
    static constexpr auto VERSION = 1;
};
serializer.setResourceVersion(1);
```

Check the version during loading:
```cpp
if (serializer.getResourceVersion() != 1) {
    // Handle version mismatch
}
```

## Tips for Students

- **Experiment with Compression**: Try different levels to see the trade-off between speed and size.
- **Test Cross-Platform**: If working on multiple systems, test with `data_ptr` to ensure compatibility.
- **Use Versioning**: Always set a version to make your data future-proof.

Check out a real-world example in [Example Walkthrough](ExampleWalkthrough.md).