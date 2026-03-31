#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"
#include "kvstore/WAL.hpp"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

void Ensure(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(1);
    }
}

std::string MakeWalPath(const std::string& name) {
    const std::string directory = "/tmp/kvstore-tests";
    ::mkdir(directory.c_str(), 0755);
    return directory + "/" + name;
}

void RemoveFileIfExists(const std::string& path) {
    ::unlink(path.c_str());
}

}  // namespace

int main() {
    kvstore::EngineOptions options;
    options.wal_path = MakeWalPath("engine_recovery.log");
    RemoveFileIfExists(options.wal_path);

    kvstore::KVStore store(options);
    Ensure(store.options().wal_path == options.wal_path, "KVStore should preserve WAL path");

    kvstore::SkipList<int, std::string> skip_list;
    Ensure(skip_list.Empty(), "skip list should start empty");

    Ensure(skip_list.Put(10, "ten"), "insert 10");
    Ensure(skip_list.Put(5, "five"), "insert 5");
    Ensure(skip_list.Put(20, "twenty"), "insert 20");
    Ensure(skip_list.Put(15, "fifteen"), "insert 15");
    Ensure(skip_list.Size() == 4, "skip list size after inserts");

    std::string value;
    Ensure(skip_list.Get(10, &value), "get existing key 10");
    Ensure(value == "ten", "value for key 10 should match");
    Ensure(!skip_list.Get(99, &value), "get missing key should fail");

    Ensure(!skip_list.Put(10, "TEN"), "updating existing key should report replace");
    Ensure(skip_list.Get(10, &value), "get updated key 10");
    Ensure(value == "TEN", "updated value should be visible");

    const auto range = skip_list.Scan(6, 20);
    Ensure(range.size() == 3, "range query should return three items");
    Ensure(range[0].first == 10 && range[0].second == "TEN", "range item 0");
    Ensure(range[1].first == 15 && range[1].second == "fifteen", "range item 1");
    Ensure(range[2].first == 20 && range[2].second == "twenty", "range item 2");

    Ensure(skip_list.Delete(15), "delete existing key");
    Ensure(!skip_list.Delete(15), "deleting same key twice should fail");
    Ensure(skip_list.Size() == 3, "skip list size after delete");

    const auto empty_range = skip_list.Scan(30, 40);
    Ensure(empty_range.empty(), "disjoint range should be empty");

    skip_list.Clear();
    Ensure(skip_list.Empty(), "skip list should be empty after clear");

    Ensure(store.Put("alpha", "1"), "engine insert alpha");
    Ensure(store.Put("beta", "2"), "engine insert beta");
    Ensure(!store.Put("alpha", "3"), "engine update alpha");

    std::string engine_value;
    Ensure(store.Get("alpha", &engine_value), "engine get alpha");
    Ensure(engine_value == "3", "engine updated alpha value");
    Ensure(store.Delete("beta"), "engine delete beta");
    Ensure(!store.Delete("missing"), "delete missing engine key");

    {
        kvstore::KVStore recovered_store(options);
        Ensure(recovered_store.Get("alpha", &engine_value), "replay should recover alpha");
        Ensure(engine_value == "3", "recovered alpha value");
        Ensure(!recovered_store.Get("beta", &engine_value), "replay should preserve delete");

        const auto engine_range = recovered_store.Scan("a", "z");
        Ensure(engine_range.size() == 1, "recovered range size");
        Ensure(engine_range[0].first == "alpha", "recovered range key");
        Ensure(engine_range[0].second == "3", "recovered range value");
    }

    const std::string wal_path = MakeWalPath("truncated_replay.log");
    RemoveFileIfExists(wal_path);
    {
        kvstore::WAL wal(wal_path);
        wal.AppendPut("key-1", "value-1");
        wal.AppendPut("key-2", "value-2");
    }
    {
        std::ofstream out(wal_path, std::ios::binary | std::ios::app);
        Ensure(out.is_open(), "open truncated wal file for append");
        out.write("broken", 6);
        out.close();
    }
    {
        kvstore::WAL wal(wal_path);
        kvstore::SkipList<std::string, std::string> replayed;
        const kvstore::ReplayStats stats = wal.Replay(
            [&replayed](const kvstore::LogRecord& record) {
                if (record.type == kvstore::RecordType::kPut) {
                    replayed.Put(record.key, record.value);
                } else {
                    replayed.Delete(record.key);
                }
            });

        Ensure(stats.applied_records == 2, "WAL replay should apply two complete records");
        Ensure(stats.skipped_tail_bytes == 6, "WAL replay should ignore truncated tail");
        Ensure(replayed.Get("key-1", &engine_value), "replayed key-1 should exist");
        Ensure(engine_value == "value-1", "replayed key-1 value");
        Ensure(replayed.Get("key-2", &engine_value), "replayed key-2 should exist");
        Ensure(engine_value == "value-2", "replayed key-2 value");
    }

    RemoveFileIfExists(options.wal_path);
    RemoveFileIfExists(wal_path);

    std::cout << "kvstore WAL and skip list tests passed\n";
    return 0;
}
