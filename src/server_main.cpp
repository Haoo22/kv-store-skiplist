#include "kvstore/kvstore.hpp"

#include <iostream>

int main() {
    kvstore::KVStore store(kvstore::EngineOptions{});

    std::cout << "KVStore server skeleton initialized. WAL path: "
              << store.options().wal_path << '\n';
    return 0;
}
