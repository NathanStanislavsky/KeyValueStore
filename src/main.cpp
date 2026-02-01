#include <iostream>
#include <cassert>
#include "kvstore.h"

int main()
{
    system("rm -f wal.log *.sst level_*_*.sst");  // Clean up

    std::cout << "=== TESTING KEY-VALUE STORE ===\n" << std::endl;

    // Test 1: Basic Put/Get
    {
        KVStore store("wal.log", ".");
        store.put("key1", "value1");
        store.put("key2", "value2");
        
        auto val1 = store.get("key1");
        assert(val1 && *val1 == "value1");
        
        auto val2 = store.get("key2");
        assert(val2 && *val2 == "value2");
        
        std::cout << "✓ Basic Put/Get works" << std::endl;
    }

    // Test 2: Persistence (restart)
    {
        KVStore store("wal.log", ".");
        auto val = store.get("key1");
        assert(val && *val == "value1");
        std::cout << "✓ Persistence works (data survives restart)" << std::endl;
    }

    // Test 3: Trigger Compaction (write many keys)
    {
        KVStore store("wal.log", ".");
        for (int i = 0; i < 5000; i++) {
            store.put("key_" + std::to_string(i), "value_" + std::to_string(i));
        }
        std::cout << "✓ Compaction triggered (check for level_* files)" << std::endl;
    }

    // Test 4: Delete (tombstone)
    {
        KVStore store("wal.log", ".");
        store.remove("key1");
        auto val = store.get("key1");
        assert(!val);  // Should return nullopt
        std::cout << "✓ Delete (tombstone) works" << std::endl;
    }

    // Test 5: Update (overwrite)
    {
        KVStore store("wal.log", ".");
        store.put("key2", "new_value2");
        auto val = store.get("key2");
        assert(val && *val == "new_value2");
        std::cout << "✓ Update works" << std::endl;
    }

    // Test 6: Non-existent key
    {
        KVStore store("wal.log", ".");
        auto val = store.get("nonexistent_key");
        assert(!val);
        std::cout << "✓ Non-existent key returns nullopt" << std::endl;
    }

    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}