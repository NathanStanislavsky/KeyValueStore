#include <iostream>
#include "kvstore.h"

int main() {
    KVStore store("data.log");

    std::cout << "Inserting 3 keys (Trigger limit is 3)..." << std::endl;
    store.put("A", "1");
    store.put("B", "2");
    store.put("C", "3"); 

    return 0;
}