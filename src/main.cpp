#include <iostream>
#include <cassert>
#include "kvstore.h"

int main()
{
    system("rm wal.log *.sst");

    std::cout << "--- PHASE 1: FILLING DATA ---" << std::endl;
    {
        KVStore store("wal.log", ".");

        for (int i = 0; i < 3000; i++)
        {
            store.put("key_" + std::to_string(i), "value_" + std::to_string(i));
        }
    }

    std::cout << "\n--- PHASE 2: TESTING BLOOM FILTER ---" << std::endl;
    {
        KVStore store("wal.log", ".");

        std::cout << "Searching for 'ghost_key' (Should skip everything)..." << std::endl;
        auto val = store.get("ghost_key");
        assert(!val);

        std::cout << "\n-----------------------------------\n";

        std::cout << "Searching for 'key_500' (Should find in data_1.sst)..." << std::endl;
        auto val2 = store.get("key_500");
        assert(val2);
    }

    std::cout << "\nTEST COMPLETE." << std::endl;
}