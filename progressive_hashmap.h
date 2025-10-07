#ifndef PROGRESSIVE_HASHMAP_H
#define PROGRESSIVE_HASHMAP_H

#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <iterator>

template<typename K, typename V>
class ProgressiveHashMap {
private:
    struct Entry {
        K key;
        V value;
        Entry* next;
        
        Entry(const K& k, const V& v) 
            : key(k), value(v), next(nullptr) {}
    };
    
    struct HashTable {
        std::vector<Entry*> buckets;
        size_t size;
        size_t mask;
        
        HashTable(size_t capacity) : size(0) {
            // Ensure capacity is power of 2
            size_t cap = 1;
            while (cap < capacity) {
                cap *= 2;
            }
            buckets.resize(cap, nullptr);
            mask = cap - 1;
        }
        
        ~HashTable() {
            for (Entry* head : buckets) {
                while (head) {
                    Entry* next = head->next;
                    delete head;
                    head = next;
                }
            }
        }
        
        // Clear buckets without deleting entries (for migration)
        void clear_without_delete() {
            buckets.clear();
        }
    };
    
    HashTable* ht1;  // Main hash table
    HashTable* ht2;  // Resizing target (nullptr when not resizing)
    size_t resizing_pos;  // Progress of incremental resizing
    bool is_shrinking;  // Track if we're shrinking or growing
    
    static const size_t INITIAL_CAPACITY = 16;
    static const size_t MIN_CAPACITY = 16;  // Don't shrink below this
    static const size_t REHASH_STEPS = 1;  // Entries to move per operation
    static constexpr double LOAD_FACTOR_HIGH = 0.75;  // Grow threshold
    static constexpr double LOAD_FACTOR_LOW = 0.25;   // Shrink threshold
    
    // Hash function - uses std::hash for the key type
    size_t hash(const K& key) const {
        return std::hash<K>{}(key);
    }
    
    Entry** lookup_bucket(HashTable* ht, const K& key, size_t h) const {
        if (!ht) return nullptr;
        
        size_t idx = h & ht->mask;
        Entry** curr = &ht->buckets[idx];
        
        while (*curr) {
            if ((*curr)->key == key) {
                return curr;
            }
            curr = &(*curr)->next;
        }
        return curr;
    }
    
    void help_resizing() {
        if (!ht2) return;  // Not resizing
        
        // Move REHASH_STEPS entries from ht1 to ht2
        size_t moved = 0;
        while (moved < REHASH_STEPS && resizing_pos < ht1->buckets.size()) {
            Entry** bucket = &ht1->buckets[resizing_pos];
            
            while (*bucket) {
                Entry* entry = *bucket;
                *bucket = entry->next;  // Remove from ht1
                
                // Insert into ht2
                size_t h = hash(entry->key);
                size_t idx = h & ht2->mask;
                entry->next = ht2->buckets[idx];
                ht2->buckets[idx] = entry;
                
                ht1->size--;
                ht2->size++;
                moved++;
            }
            resizing_pos++;
        }
        
        // Check if resizing is complete
        if (resizing_pos >= ht1->buckets.size()) {
            // Clear old table without deleting entries (they're in ht2 now)
            ht1->clear_without_delete();
            delete ht1;
            ht1 = ht2;
            ht2 = nullptr;
            resizing_pos = 0;
            is_shrinking = false;
        }
    }
    
    void start_resizing(bool shrink = false) {
        assert(!ht2);
        
        size_t new_capacity;
        if (shrink) {
            new_capacity = ht1->buckets.size() / 2;
            if (new_capacity < MIN_CAPACITY) {
                return;  // Don't shrink below minimum
            }
            is_shrinking = true;
        } else {
            new_capacity = ht1->buckets.size() * 2;
            is_shrinking = false;
        }
        
        ht2 = new HashTable(new_capacity);
        resizing_pos = 0;
    }
    
    void check_load_factor() {
        // Only check if we're not already resizing
        if (ht2) return;
        
        // Calculate load factor based on current table
        double load = static_cast<double>(ht1->size) / ht1->buckets.size();
        
        if (load > LOAD_FACTOR_HIGH) {
            start_resizing(false);  // Grow
        } else if (load < LOAD_FACTOR_LOW && ht1->buckets.size() > MIN_CAPACITY) {
            start_resizing(true);   // Shrink
        }
    }
    
    // Determine which table should receive new insertions during resize
    HashTable* get_insert_table(const K& key) {
        if (!ht2) return ht1;
        
        // If resizing, check if key's bucket in ht1 has been migrated
        size_t h = hash(key);
        size_t ht1_idx = h & ht1->mask;
        
        // If this bucket has already been processed, insert into ht2
        if (ht1_idx < resizing_pos) {
            return ht2;
        }
        
        // Otherwise, insert into ht1 (will be migrated later)
        return ht1;
    }

public:
    // Iterator implementation
    class iterator {
    private:
        friend class ProgressiveHashMap;
        
        ProgressiveHashMap* map;
        HashTable* current_table;
        size_t bucket_idx;
        Entry* current_entry;
        bool in_ht2;
        
        iterator(ProgressiveHashMap* m, HashTable* ht, size_t idx, Entry* entry, bool ht2_flag)
            : map(m), current_table(ht), bucket_idx(idx), current_entry(entry), in_ht2(ht2_flag) {
            if (current_entry == nullptr) {
                advance_to_next();
            }
        }
        
        void advance_to_next() {
            // Try next entry in chain
            if (current_entry && current_entry->next) {
                current_entry = current_entry->next;
                return;
            }
            
            // Find next non-empty bucket
            if (current_table) {
                bucket_idx++;
                while (bucket_idx < current_table->buckets.size()) {
                    if (current_table->buckets[bucket_idx]) {
                        current_entry = current_table->buckets[bucket_idx];
                        return;
                    }
                    bucket_idx++;
                }
            }
            
            // If we're in ht2 or no ht2 exists, we're done
            if (in_ht2 || !map->ht2) {
                current_table = nullptr;
                current_entry = nullptr;
                bucket_idx = 0;
                return;
            }
            
            // Switch to ht1
            current_table = map->ht1;
            bucket_idx = 0;
            in_ht2 = false;
            
            while (bucket_idx < current_table->buckets.size()) {
                if (current_table->buckets[bucket_idx]) {
                    current_entry = current_table->buckets[bucket_idx];
                    return;
                }
                bucket_idx++;
            }
            
            // Reached end
            current_table = nullptr;
            current_entry = nullptr;
        }
        
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const K, V>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        
        iterator() : map(nullptr), current_table(nullptr), bucket_idx(0), current_entry(nullptr), in_ht2(false) {}
        
        std::pair<const K&, V&> operator*() {
            assert(current_entry);
            return {current_entry->key, current_entry->value};
        }
        
        std::pair<const K*, V*> operator->() {
            assert(current_entry);
            return {&current_entry->key, &current_entry->value};
        }
        
        iterator& operator++() {
            advance_to_next();
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            advance_to_next();
            return tmp;
        }
        
        bool operator==(const iterator& other) const {
            return current_entry == other.current_entry && 
                   current_table == other.current_table &&
                   bucket_idx == other.bucket_idx;
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };
    
    // Const iterator
    class const_iterator {
    private:
        friend class ProgressiveHashMap;
        
        const ProgressiveHashMap* map;
        const HashTable* current_table;
        size_t bucket_idx;
        const Entry* current_entry;
        bool in_ht2;
        
        const_iterator(const ProgressiveHashMap* m, const HashTable* ht, size_t idx, const Entry* entry, bool ht2_flag)
            : map(m), current_table(ht), bucket_idx(idx), current_entry(entry), in_ht2(ht2_flag) {
            if (current_entry == nullptr) {
                advance_to_next();
            }
        }
        
        void advance_to_next() {
            // Try next entry in chain
            if (current_entry && current_entry->next) {
                current_entry = current_entry->next;
                return;
            }
            
            // Find next non-empty bucket
            if (current_table) {
                bucket_idx++;
                while (bucket_idx < current_table->buckets.size()) {
                    if (current_table->buckets[bucket_idx]) {
                        current_entry = current_table->buckets[bucket_idx];
                        return;
                    }
                    bucket_idx++;
                }
            }
            
            // If we're in ht2 or no ht2 exists, we're done
            if (in_ht2 || !map->ht2) {
                current_table = nullptr;
                current_entry = nullptr;
                bucket_idx = 0;
                return;
            }
            
            // Switch to ht1
            current_table = map->ht1;
            bucket_idx = 0;
            in_ht2 = false;
            
            while (bucket_idx < current_table->buckets.size()) {
                if (current_table->buckets[bucket_idx]) {
                    current_entry = current_table->buckets[bucket_idx];
                    return;
                }
                bucket_idx++;
            }
            
            // Reached end
            current_table = nullptr;
            current_entry = nullptr;
        }
        
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const K, V>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        
        const_iterator() : map(nullptr), current_table(nullptr), bucket_idx(0), current_entry(nullptr), in_ht2(false) {}
        
        std::pair<const K&, const V&> operator*() const {
            assert(current_entry);
            return {current_entry->key, current_entry->value};
        }
        
        std::pair<const K*, const V*> operator->() const {
            assert(current_entry);
            return {&current_entry->key, &current_entry->value};
        }
        
        const_iterator& operator++() {
            advance_to_next();
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            advance_to_next();
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            return current_entry == other.current_entry && 
                   current_table == other.current_table &&
                   bucket_idx == other.bucket_idx;
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };
    
    ProgressiveHashMap() : ht2(nullptr), resizing_pos(0), is_shrinking(false) {
        ht1 = new HashTable(INITIAL_CAPACITY);
    }
    
    ~ProgressiveHashMap() {
        delete ht1;
        if (ht2) delete ht2;
    }
    
    // Iterator methods
    iterator begin() {
        // Start from ht2 if it exists (has migrated entries)
        if (ht2 && ht2->size > 0) {
            for (size_t i = 0; i < ht2->buckets.size(); i++) {
                if (ht2->buckets[i]) {
                    return iterator(this, ht2, i, ht2->buckets[i], true);
                }
            }
        }
        
        // Otherwise start from ht1
        for (size_t i = 0; i < ht1->buckets.size(); i++) {
            if (ht1->buckets[i]) {
                return iterator(this, ht1, i, ht1->buckets[i], false);
            }
        }
        
        return end();
    }
    
    iterator end() {
        return iterator(this, nullptr, 0, nullptr, false);
    }
    
    const_iterator begin() const {
        // Start from ht2 if it exists (has migrated entries)
        if (ht2 && ht2->size > 0) {
            for (size_t i = 0; i < ht2->buckets.size(); i++) {
                if (ht2->buckets[i]) {
                    return const_iterator(this, ht2, i, ht2->buckets[i], true);
                }
            }
        }
        
        // Otherwise start from ht1
        for (size_t i = 0; i < ht1->buckets.size(); i++) {
            if (ht1->buckets[i]) {
                return const_iterator(this, ht1, i, ht1->buckets[i], false);
            }
        }
        
        return end();
    }
    
    const_iterator end() const {
        return const_iterator(this, nullptr, 0, nullptr, false);
    }
    
    const_iterator cbegin() const {
        return begin();
    }
    
    const_iterator cend() const {
        return end();
    }
    
    // Lookup a key, returns nullptr if not found
    V* lookup(const K& key) {
        help_resizing();  // Make progress on resizing
        
        size_t h = hash(key);
        
        // Check ht2 first if resizing (migrated entries are here)
        if (ht2) {
            Entry** entry_ptr = lookup_bucket(ht2, key, h);
            if (entry_ptr && *entry_ptr) {
                return &(*entry_ptr)->value;
            }
        }
        
        // Check ht1
        Entry** entry_ptr = lookup_bucket(ht1, key, h);
        if (entry_ptr && *entry_ptr) {
            return &(*entry_ptr)->value;
        }
        
        return nullptr;
    }
    
    // Const version of lookup
    const V* lookup(const K& key) const {
        size_t h = hash(key);
        
        // Check ht2 first if resizing (migrated entries are here)
        if (ht2) {
            Entry** entry_ptr = lookup_bucket(ht2, key, h);
            if (entry_ptr && *entry_ptr) {
                return &(*entry_ptr)->value;
            }
        }
        
        // Check ht1
        Entry** entry_ptr = lookup_bucket(ht1, key, h);
        if (entry_ptr && *entry_ptr) {
            return &(*entry_ptr)->value;
        }
        
        return nullptr;
    }
    
    // Insert or update a key-value pair
    void set(const K& key, const V& value) {
        help_resizing();  // Make progress on resizing
        
        size_t h = hash(key);
        
        // Try to update existing entry in ht2 first
        if (ht2) {
            Entry** entry_ptr = lookup_bucket(ht2, key, h);
            if (entry_ptr && *entry_ptr) {
                (*entry_ptr)->value = value;
                return;
            }
        }
        
        // Try to update existing entry in ht1
        Entry** entry_ptr = lookup_bucket(ht1, key, h);
        if (entry_ptr && *entry_ptr) {
            (*entry_ptr)->value = value;
            return;
        }
        
        // Insert new entry into appropriate table
        HashTable* target = get_insert_table(key);
        size_t idx = h & target->mask;
        
        Entry* new_entry = new Entry(key, value);
        new_entry->next = target->buckets[idx];
        target->buckets[idx] = new_entry;
        target->size++;
        
        check_load_factor();
    }
    
    // Delete a key, returns true if found and deleted
    bool del(const K& key) {
        help_resizing();  // Make progress on resizing
        
        size_t h = hash(key);
        
        // Try ht2 first if resizing
        if (ht2) {
            Entry** entry_ptr = lookup_bucket(ht2, key, h);
            if (entry_ptr && *entry_ptr) {
                Entry* to_delete = *entry_ptr;
                *entry_ptr = to_delete->next;
                delete to_delete;
                ht2->size--;
                check_load_factor();  // Check for shrinking
                return true;
            }
        }
        
        // Try ht1
        Entry** entry_ptr = lookup_bucket(ht1, key, h);
        if (entry_ptr && *entry_ptr) {
            Entry* to_delete = *entry_ptr;
            *entry_ptr = to_delete->next;
            delete to_delete;
            ht1->size--;
            check_load_factor();  // Check for shrinking
            return true;
        }
        
        return false;
    }
    
    // Get total number of entries
    size_t size() const {
        return ht1->size + (ht2 ? ht2->size : 0);
    }
    
    // Check if empty
    bool empty() const {
        return size() == 0;
    }
    
    // Get current capacity
    size_t capacity() const {
        return ht1->buckets.size();
    }
    
    // Get current load factor
    double load_factor() const {
        return static_cast<double>(size()) / capacity();
    }
    
    // Check if resizing is in progress
    bool is_resizing() const {
        return ht2 != nullptr;
    }
    
    // Check if key exists
    bool contains(const K& key) const {
        return lookup(key) != nullptr;
    }
    
    // Operator[] for convenient access (creates entry if doesn't exist)
    V& operator[](const K& key) {
        V* val = lookup(key);
        if (val) {
            return *val;
        }
        set(key, V());  // Insert default-constructed value
        return *lookup(key);  // Return reference to newly inserted value
    }
    
    // Clear all entries
    void clear() {
        delete ht1;
        if (ht2) {
            delete ht2;
            ht2 = nullptr;
        }
        ht1 = new HashTable(INITIAL_CAPACITY);
        resizing_pos = 0;
        is_shrinking = false;
    }
};

#endif // PROGRESSIVE_HASHMAP_