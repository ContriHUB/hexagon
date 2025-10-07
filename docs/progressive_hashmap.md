# Progressive HashMap Implementation

## Overview

This is a C++ implementation of a **progressive (incremental) hash table** that performs resizing operations gradually instead of all at once. When the load factor exceeds a threshold, the hash table doesn't freeze to rehash all entries immediately. Instead, it creates a new larger table and migrates entries incrementally over subsequent operations.

**This is a generic template-based implementation that works with any key and value types.**

## Key Features

- **Generic Template**: Works with any key-value types (e.g., `<std::string, int>`, `<int, MyStruct>`)
- **Incremental Resizing**: No single operation causes a large pause
- **Automatic Downsizing**: Shrinks when load factor drops below threshold
- **Iterator Support**: Full STL-compatible forward iterators
- **Dual Table Architecture**: Uses two hash tables during resize operations
- **Amortized O(1) Operations**: All operations remain fast even during resizing
- **Separate Chaining**: Collision resolution using linked lists
- **Power-of-2 Sizing**: Tables are always sized as powers of 2 for fast modulo operations
- **STL Compatible**: Uses `std::hash<K>` for hashing any standard type

## Architecture

### Template Declaration

```cpp
template<typename K, typename V>
class ProgressiveHashMap;
```

- **K**: Key type (must be hashable with `std::hash<K>`)
- **V**: Value type (any copyable type)

### Data Structures

#### Entry
```cpp
template<typename K, typename V>
struct Entry {
    K key;
    V value;
    Entry* next;  // For chaining collisions
};
```

#### HashTable
```cpp
struct HashTable {
    std::vector<Entry*> buckets;  // Array of bucket heads
    size_t size;                   // Number of entries
    size_t mask;                   // capacity - 1 (for fast modulo)
};
```

#### Main State
- `ht1`: Primary hash table (always active)
- `ht2`: Secondary table (only during resizing, nullptr otherwise)
- `resizing_pos`: Tracks which bucket in ht1 is being migrated next
- `is_shrinking`: Flag indicating if current resize is shrinking or growing

## Core Operations

### 1. Lookup
```cpp
V* lookup(const K& key)
const V* lookup(const K& key) const
```

**Purpose**: Find a key and return pointer to its value, or nullptr if not found.

**Algorithm**:
1. Help with ongoing resize (migrate REHASH_STEPS entries) - non-const version only
2. Compute hash of the key using `std::hash<K>`
3. If resizing, check ht2 first (migrated entries are here)
4. Then check ht1
5. Return pointer to value if found, nullptr otherwise

**Time Complexity**:
- **Average Case**: O(1)
- **Worst Case**: O(n) if all keys hash to same bucket
- **Amortized**: O(1) including resize work

**Example**:
```cpp
ProgressiveHashMap<std::string, int> map;
map.set("count", 42);
int* val = map.lookup("count");
if (val) {
    std::cout << *val << std::endl;  // Prints: 42
}
```

### 2. Set (Insert/Update)
```cpp
void set(const K& key, const V& value)
```

**Purpose**: Insert a new key-value pair or update existing key.

**Algorithm**:
1. Help with ongoing resize
2. Compute hash of the key
3. Check if key exists in ht2 (if resizing) and update if found
4. Check if key exists in ht1 and update if found
5. If new key, determine target table:
   - If not resizing: insert into ht1
   - If resizing and bucket migrated: insert into ht2
   - If resizing and bucket not migrated: insert into ht1
6. Insert new entry at head of chain
7. Check load factor and start resize/shrink if needed

**Time Complexity**:
- **Average Case**: O(1)
- **Worst Case**: O(n)
- **Amortized**: O(1)

**Example**:
```cpp
ProgressiveHashMap<int, std::string> map;
map.set(1, "one");
map.set(2, "two");
map.set(1, "ONE");  // Updates existing key
```

### 3. Delete
```cpp
bool del(const K& key)
```

**Purpose**: Remove a key-value pair from the map.

**Algorithm**:
1. Help with ongoing resize
2. Compute hash of the key
3. Search in ht2 first (if resizing)
4. Search in ht1
5. If found, unlink from chain and delete
6. Check load factor for potential downsizing
7. Return true if deleted, false if not found

**Time Complexity**:
- **Average Case**: O(1)
- **Worst Case**: O(n)
- **Amortized**: O(1)

**Example**:
```cpp
bool deleted = map.del(2);  // Returns true if key existed
```

### 4. Size
```cpp
size_t size() const
```

**Purpose**: Return total number of entries across both tables.

**Time Complexity**: O(1)

### 5. Contains
```cpp
bool contains(const K& key) const
```

**Purpose**: Check if a key exists in the map.

**Time Complexity**: O(1) average

**Example**:
```cpp
if (map.contains("key1")) {
    // Key exists
}
```

### 6. Operator[]
```cpp
V& operator[](const K& key)
```

**Purpose**: Access or insert with default value (like `std::map`).

**Behavior**:
- If key exists, returns reference to value
- If key doesn't exist, inserts default-constructed `V()` and returns reference

**Time Complexity**: O(1) average

**Example**:
```cpp
ProgressiveHashMap<std::string, int> counter;
counter["apple"]++;  // Inserts with 0, then increments to 1
counter["apple"]++;  // Now 2
```

### 7. Iterator Support
```cpp
iterator begin()
iterator end()
const_iterator begin() const
const_iterator end() const
const_iterator cbegin() const
const_iterator cend() const
```

**Purpose**: Iterate over all key-value pairs in the map.

**Features**:
- Forward iterator (read and increment)
- Compatible with range-based for loops
- Works with STL algorithms
- Returns `std::pair<const K&, V&>` when dereferenced

**Time Complexity**: O(n) to iterate all entries

**Examples**:
```cpp
ProgressiveHashMap<std::string, int> map;
map.set("a", 1);
map.set("b", 2);
map.set("c", 3);

// Range-based for loop
for (auto [key, value] : map) {
    std::cout << key << ": " << value << std::endl;
}

// Iterator loop
for (auto it = map.begin(); it != map.end(); ++it) {
    auto [key, value] = *it;
    value += 10;  // Modify values
}

// STL algorithms
auto count = std::count_if(map.begin(), map.end(), 
    [](auto pair) { 
        auto [key, val] = pair;
        return val > 5; 
    });

// Const iteration
const auto& cmap = map;
for (auto [key, value] : cmap) {
    std::cout << key << ": " << value << std::endl;
}
```

### 8. Additional Helper Methods

#### Empty
```cpp
bool empty() const
```
Check if the map contains no entries.

#### Capacity
```cpp
size_t capacity() const
```
Get the current bucket count (not the number of entries).

#### Load Factor
```cpp
double load_factor() const
```
Get the current load factor (size / capacity).

#### Is Resizing
```cpp
bool is_resizing() const
```
Check if a resize operation is currently in progress.

#### Clear
```cpp
void clear()
```
Remove all entries and reset to initial capacity.

**Example**:
```cpp
ProgressiveHashMap<int, std::string> map;
// ... add entries ...

std::cout << "Size: " << map.size() << std::endl;
std::cout << "Capacity: " << map.capacity() << std::endl;
std::cout << "Load: " << map.load_factor() << std::endl;
std::cout << "Resizing: " << (map.is_resizing() ? "yes" : "no") << std::endl;

if (!map.empty()) {
    map.clear();
}
```

## Progressive Resizing Mechanism

### When Resizing Triggers

**Growing**:
- **Load Factor Threshold**: 0.75 (75% capacity)
- Triggered after insertions when size/capacity > 0.75
- Doubles the capacity

**Shrinking**:
- **Load Factor Threshold**: 0.25 (25% capacity)
- Triggered after deletions when size/capacity < 0.25
- Halves the capacity
- Never shrinks below MIN_CAPACITY (16)

**Formula**: `load = size / capacity`

### Resize Process

1. **Start Resizing**: Create ht2 with new capacity (double or half)
2. **Incremental Migration**: Each operation moves REHASH_STEPS entries (currently 1)
3. **Bucket-by-Bucket**: Process buckets sequentially from index 0
4. **Complete**: When all buckets migrated, delete ht1 and promote ht2

### Key Decisions During Resize

**Which table to search?**
- Check ht2 first (has migrated entries)
- Then check ht1 (has pending entries)

**Which table for new insertions?**
- If bucket index < resizing_pos: insert into ht2 (already migrated)
- Otherwise: insert into ht1 (will migrate later)

**Why this works:**
- Ensures every key is in exactly one table
- New insertions don't get "lost" during migration
- Old table shrinks, new table grows

## Hash Function

Uses **std::hash<K>** for any standard type:
```cpp
size_t hash(const K& key) const {
    return std::hash<K>{}(key);
}
```

**Supported Types** (with std::hash specializations):
- All integral types: `int`, `long`, `size_t`, etc.
- `std::string`
- Pointers
- Custom types (if you provide `std::hash` specialization)

**Custom Hash Example**:
```cpp
struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Specialize std::hash
namespace std {
    template<>
    struct hash<Point> {
        size_t operator()(const Point& p) const {
            return hash<int>{}(p.x) ^ (hash<int>{}(p.y) << 1);
        }
    };
}

// Now you can use it
ProgressiveHashMap<Point, std::string> pointMap;
```

## Complexity Analysis

| Operation | Average | Worst | Amortized | Space |
|-----------|---------|-------|-----------|-------|
| lookup    | O(1)    | O(n)  | O(1)      | -     |
| set       | O(1)    | O(n)  | O(1)      | -     |
| del       | O(1)    | O(n)  | O(1)      | -     |
| contains  | O(1)    | O(n)  | O(1)      | -     |
| operator[]| O(1)    | O(n)  | O(1)      | -     |
| size      | O(1)    | O(1)  | O(1)      | -     |
| empty     | O(1)    | O(1)  | O(1)      | -     |
| capacity  | O(1)    | O(1)  | O(1)      | -     |
| begin/end | O(1)    | O(n)  | O(1)      | -     |
| iterator++| O(1)    | O(n)  | O(1)      | -     |
| Iteration | O(n)    | O(n)  | O(n)      | -     |
| Resize    | -       | -     | O(1) per op | O(n) |

**Space Complexity**: 
- Normal: O(n)
- During resize: O(2n) = O(n) (two tables)

### Amortized Analysis of Resizing

**Growing**:
- Cost to migrate all entries: O(n)
- Operations before next resize: Ω(n) (must insert n/2 more entries)
- Amortized cost per operation: O(n)/Ω(n) = O(1)

**Shrinking**:
- Cost to migrate all entries: O(n)
- Operations before shrink: Ω(n) (must delete 3n/4 entries)
- Amortized cost per operation: O(n)/Ω(n) = O(1)

## Configuration Parameters

```cpp
static const size_t INITIAL_CAPACITY = 16;       // Starting size
static const size_t MIN_CAPACITY = 16;           // Minimum size (no shrinking below)
static const size_t REHASH_STEPS = 1;            // Entries moved per operation
static constexpr double LOAD_FACTOR_HIGH = 0.75; // Grow threshold
static constexpr double LOAD_FACTOR_LOW = 0.25;  // Shrink threshold
```

**Tuning Considerations**:
- Higher REHASH_STEPS: Faster resize completion, more variable operation times
- Lower REHASH_STEPS: More consistent operation times, slower resize
- Higher LOAD_FACTOR_HIGH: Less memory, worse chain lengths
- Lower LOAD_FACTOR_HIGH: More memory, better performance
- Higher LOAD_FACTOR_LOW: More shrinking operations
- Lower LOAD_FACTOR_LOW: Less memory reclamation

## Advantages Over Traditional Hashmap

1. **No Large Pauses**: Traditional resize blocks for O(n) time
2. **Predictable Performance**: Every operation is fast
3. **Real-time Suitable**: Good for latency-sensitive applications
4. **Graceful Degradation**: Works well even under heavy load
5. **Type Safe**: Template ensures compile-time type checking
6. **Flexible**: Works with any key-value types
7. **Memory Efficient**: Automatic downsizing reclaims unused memory
8. **STL Compatible**: Works with range-based for and algorithms

## Usage Examples

### Basic String-String Map
```cpp
ProgressiveHashMap<std::string, std::string> map;

// Insert
map.set("key1", "value1");
map.set("key2", "value2");

// Lookup
std::string* val = map.lookup("key1");
if (val) {
    std::cout << *val << std::endl;  // Prints: value1
}

// Update
map.set("key1", "new_value");

// Delete
bool deleted = map.del("key2");

// Iterate
for (auto [key, value] : map) {
    std::cout << key << " => " << value << std::endl;
}

// Check resize status
bool resizing = map.is_resizing();
```

### Integer-Integer Map (Counter)
```cpp
ProgressiveHashMap<int, int> counter;
counter[1] = 100;
counter[2] = 200;
counter[1]++;  // Now 101

// Iterate and sum
int total = 0;
for (auto [key, val] : counter) {
    total += val;
}
```

### String to Complex Object
```cpp
struct UserData {
    std::string name;
    int age;
    std::vector<std::string> tags;
};

ProgressiveHashMap<std::string, UserData> users;

UserData user{"Alice", 30, {"admin", "developer"}};
users.set("alice123", user);

UserData* found = users.lookup("alice123");
if (found) {
    std::cout << found->name << " is " << found->age << std::endl;
}

// Iterate users
for (auto [id, user] : users) {
    std::cout << id << ": " << user.name << std::endl;
}
```

### With Custom Key Type
```cpp
struct UserID {
    int id;
    std::string domain;
    
    bool operator==(const UserID& other) const {
        return id == other.id && domain == other.domain;
    }
};

// Specialize std::hash
namespace std {
    template<>
    struct hash<UserID> {
        size_t operator()(const UserID& uid) const {
            return hash<int>{}(uid.id) ^ hash<string>{}(uid.domain);
        }
    };
}

ProgressiveHashMap<UserID, std::string> userNames;
userNames.set(UserID{123, "company.com"}, "Alice");
```

### Real-World: Key-Value Server Entry Storage
```cpp
struct Entry {
    std::string value;
    std::chrono::steady_clock::time_point expires_at;
    size_t access_count;
    bool has_ttl;
};

// Store complete entries, not just strings
ProgressiveHashMap<std::string, Entry> cache;

Entry entry;
entry.value = "cached_data";
entry.has_ttl = false;
entry.access_count = 0;

cache.set("user:1234", entry);

// Direct access to Entry object
Entry* found = cache.lookup("user:1234");
if (found) {
    found->access_count++;
    std::cout << found->value << std::endl;
}

// Iterate and clean expired entries
auto now = std::chrono::steady_clock::now();
for (auto [key, entry] : cache) {
    if (entry.has_ttl && now > entry.expires_at) {
        cache.del(key);
    }
}
```

### Demonstrating Automatic Downsizing
```cpp
ProgressiveHashMap<int, std::string> map;

// Fill it up (triggers growth)
for (int i = 0; i < 1000; i++) {
    map.set(i, "value" + std::to_string(i));
}
std::cout << "After insertions - Capacity: " << map.capacity() << std::endl;
// Capacity will be large (e.g., 2048)

// Delete most entries (triggers shrinking)
for (int i = 0; i < 950; i++) {
    map.del(i);
}
std::cout << "After deletions - Capacity: " << map.capacity() << std::endl;
// Capacity will automatically shrink (e.g., to 128 or 64)

std::cout << "Load factor: " << map.load_factor() << std::endl;
// Load factor will be healthy again
```

## Memory Management

- Entries allocated with `new`, freed with `delete`
- HashTable destructor properly cleans up all entries
- During resize, entries are moved (not copied), so no double-free issues
- Value types should have proper copy constructors/assignment operators
- Destructor automatically called for all entries when map is destroyed
- Automatic downsizing prevents memory waste from deleted entries

## Comparison with std::unordered_map

| Feature | ProgressiveHashMap | std::unordered_map |
|---------|-------------------|--------------------|
| Average Lookup | O(1) | O(1) |
| Worst Lookup | O(n) | O(n) |
| Resize Pause | None (incremental) | O(n) pause |
| Downsizing | Automatic | Manual (rehash) |
| Iterator Support | Yes (forward) | Yes (forward) |
| Iterator Invalidation | On resize | On rehash |
| Template-based | Yes | Yes |
| Predictable Latency | Yes | No (during resize) |
| Memory Overhead | Low | Low |
| Range-based for | Yes | Yes |

## Comparison with std::map

| Feature | ProgressiveHashMap | std::map |
|---------|-------------------|----------|
| Average Lookup | O(1) | O(log n) |
| Worst Lookup | O(n) | O(log n) |
| Ordered | No | Yes |
| Resize Pause | None | N/A (tree-based) |
| Memory | O(n) | O(n) |
| Iterator Type | Forward | Bidirectional |

## Iterator Behavior

### Iterator Traversal Order

The iterator visits entries in this order:
1. All entries in ht2 (if resizing and has migrated entries)
2. All entries in ht1
3. Within each table: bucket by bucket, then through chains

**Note**: Order is not deterministic and may change during resizing.

### Iterator Invalidation

Iterators are invalidated when:
- The resize completes (ht1 and ht2 swap)
- The key being pointed to is deleted

**Safe operations during iteration**:
- Reading values
- Modifying values (not keys)

**Unsafe operations during iteration**:
- Inserting new entries
- Deleting entries (except the current one)

**Example of safe iteration**:
```cpp
// Safe: modify values
for (auto [key, value] : map) {
    value++;  // OK
}

// Unsafe: insert during iteration
for (auto [key, value] : map) {
    map.set("new_key", 42);  // May invalidate iterator
}

// Safe: collect keys first, then modify
std::vector<std::string> keys_to_delete;
for (auto [key, value] : map) {
    if (should_delete(value)) {
        keys_to_delete.push_back(key);
    }
}
for (const auto& key : keys_to_delete) {
    map.del(key);
}
```


## Conclusion

This progressive hashmap implementation successfully provides O(1) amortized operations without large pause times during resizing. The template-based design makes it flexible for any key-value types while maintaining type safety. 

**Key advantages**:
- **Predictable latency** through incremental resizing
- **Memory efficiency** through automatic downsizing
- **Full STL integration** with iterators and range-based for
- **Type flexibility** for any hashable key and copyable value types

It's well-suited for applications requiring consistent performance, such as:
- Key-value servers and caches
- Real-time systems
- Game engines
- Financial trading systems
- Any latency-sensitive application

The ability to store complex value types (like structs with multiple fields) and iterate over all entries makes it far more practical than basic string-only hashmaps, enabling direct storage of application domain objects without serialization overhead.