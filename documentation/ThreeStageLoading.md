# Three-Stage Loading in xserializer

`xserializer` uses a three-stage loading process to make loading data efficient and flexible, especially for complex systems like games. Think of it like unpacking a suitcase in steps: first, you check the label, then you unpack the contents, and finally, you organize everything.

## The Three Stages

1. **Header Loading** (`LoadHeader`):
   - Reads the file’s header, which contains metadata like file version and data size.
   - This is quick and lets you check if the file is valid before loading everything.
   - Example: `serializer.LoadHeader(file, sizeof(MyStruct))`.

2. **Object Loading** (`LoadObject`):
   - Loads the actual data into memory, resolving pointers and decompressing data.
   - Allocates memory efficiently as a single block (unless marked `m_bUnique`).
   - Example: `void* data = serializer.LoadObject(file)`.

3. **Resolution** (`ResolveObject`):
   - Calls a special constructor to finalize the object, allowing you to register data with other systems (e.g., VRAM or animation managers).
   - Example: `serializer.ResolveObject(loadedObj)`.

## Why Three Stages?

- **Efficiency**: Header loading is fast, so you can skip invalid files early.
- **Flexibility**: Resolution lets you integrate with other systems (e.g., game engines) after loading.
- **Safety**: Separating steps ensures thread safety and reduces errors.

## Example

```cpp
struct Player {
    int health;
    Player(xserializer::stream& Stream) noexcept {
        // Called during resolution to register with game systems
    }
};

namespace xserializer::io_functions {
    xerr SerializeIO(xserializer::stream& Stream, const Player& P) noexcept {
        Stream.Serialize(P.health);
        return {};
    }
}

int main() {
    xserializer::stream serializer;
    xfile::stream file;
    file.open(L"player.bin", "r");
    serializer.LoadHeader(file, sizeof(Player));
    Player* player = (Player*)serializer.LoadObject(file);
    serializer.ResolveObject(player); // Calls Player’s constructor
    xserializer::default_memory_handler_v.Free(xserializer::mem_type{ .m_bUnique = true }, player);
    file.close();
    return 0;
}
```

## Tips for Students

- **Start with `Load`**: The `Load` function combines all three stages for simplicity.
- **Use Resolution for Setup**: If your data needs setup (e.g., registering with a game engine), use the resolution constructor.
- **Check Header First**: For large files, check the header to avoid loading invalid data.

Learn about advanced features like compression in [Advanced Usage](AdvancedUsage.md).