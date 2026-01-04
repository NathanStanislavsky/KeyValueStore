#include <iostream>
#include <cassert>
#include "wal.h"
#include "memtable.h"
#include "kvstore.h"

using namespace std;

int main() {
    KVStore store("test.log");

    std::cout << "putting data..." << std::endl;
    store.put("user:1", "Nathan");
    store.put("user:2", "Emma");

    std::cout << "getting data..." << std::endl;
    auto result = store.get("user:1");

    if (result) {
        std:cout << "Found: " << *result << std::endl;
    }

    return 0;
}