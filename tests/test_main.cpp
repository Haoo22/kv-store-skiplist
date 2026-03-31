#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    kvstore::EngineOptions options;
    options.wal_path = "tests/test_wal.log";

    kvstore::KVStore store(options);
    assert(store.options().wal_path == "tests/test_wal.log");

    kvstore::SkipList<int, std::string> skip_list;
    assert(skip_list.Empty());

    assert(skip_list.Put(10, "ten"));
    assert(skip_list.Put(5, "five"));
    assert(skip_list.Put(20, "twenty"));
    assert(skip_list.Put(15, "fifteen"));
    assert(skip_list.Size() == 4);

    std::string value;
    assert(skip_list.Get(10, &value));
    assert(value == "ten");
    assert(!skip_list.Get(99, &value));

    assert(!skip_list.Put(10, "TEN"));
    assert(skip_list.Get(10, &value));
    assert(value == "TEN");

    const auto range = skip_list.Scan(6, 20);
    assert(range.size() == 3);
    assert(range[0].first == 10 && range[0].second == "TEN");
    assert(range[1].first == 15 && range[1].second == "fifteen");
    assert(range[2].first == 20 && range[2].second == "twenty");

    assert(skip_list.Delete(15));
    assert(!skip_list.Delete(15));
    assert(skip_list.Size() == 3);

    const auto empty_range = skip_list.Scan(30, 40);
    assert(empty_range.empty());

    skip_list.Clear();
    assert(skip_list.Empty());

    std::cout << "kvstore skip list test passed\n";
    return 0;
}
