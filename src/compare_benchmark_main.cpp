#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

enum class WorkloadMode {
    kMixed,
    kReadHeavy,
    kWriteHeavy,
};

struct BenchmarkResult {
    std::string name;
    int thread_count {0};
    int operations_per_thread {0};
    long long total_operations {0};
    double seconds {0.0};
    double throughput_ops {0.0};
    double average_latency_ns {0.0};
    std::size_t final_size {0};
};

struct MapStore {
    bool Put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto result = data_.insert(std::make_pair(key, value));
        if (!result.second) {
            result.first->second = value;
        }
        return result.second;
    }

    bool Get(const std::string& key, std::string* value) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto iterator = data_.find(key);
        if (iterator == data_.end()) {
            return false;
        }
        if (value != nullptr) {
            *value = iterator->second;
        }
        return true;
    }

    bool Delete(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.erase(key) > 0;
    }

    std::size_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

private:
    std::mutex mutex_;
    std::map<std::string, std::string> data_;
};

class ShardedMapStore {
public:
    explicit ShardedMapStore(std::size_t shard_count = 16)
        : shards_(shard_count == 0 ? 1 : shard_count) {}

    bool Put(const std::string& key, const std::string& value) {
        Shard& shard = ShardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        const auto result = shard.data.insert(std::make_pair(key, value));
        if (!result.second) {
            result.first->second = value;
        }
        return result.second;
    }

    bool Get(const std::string& key, std::string* value) {
        Shard& shard = ShardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        const auto iterator = shard.data.find(key);
        if (iterator == shard.data.end()) {
            return false;
        }
        if (value != nullptr) {
            *value = iterator->second;
        }
        return true;
    }

    bool Delete(const std::string& key) {
        Shard& shard = ShardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        return shard.data.erase(key) > 0;
    }

    std::size_t Size() {
        std::size_t total = 0;
        for (Shard& shard : shards_) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            total += shard.data.size();
        }
        return total;
    }

private:
    struct Shard {
        std::mutex mutex;
        std::map<std::string, std::string> data;
    };

    Shard& ShardFor(const std::string& key) {
        return shards_[ShardIndex(key)];
    }

    std::size_t ShardIndex(const std::string& key) const {
        return hasher_(key) % shards_.size();
    }

    std::vector<Shard> shards_;
    std::hash<std::string> hasher_;
};

class ShardedSkipListStore {
public:
    explicit ShardedSkipListStore(std::size_t shard_count = 16)
        : shards_(shard_count == 0 ? 1 : shard_count) {
        for (std::unique_ptr<kvstore::SkipList<std::string, std::string>>& shard : shards_) {
            shard = std::make_unique<kvstore::SkipList<std::string, std::string>>();
        }
    }

    bool Put(const std::string& key, const std::string& value) {
        return ShardFor(key).Put(key, value);
    }

    bool Get(const std::string& key, std::string* value) {
        return ShardFor(key).Get(key, value);
    }

    bool Delete(const std::string& key) {
        return ShardFor(key).Delete(key);
    }

    std::size_t Size() {
        std::size_t total = 0;
        for (const std::unique_ptr<kvstore::SkipList<std::string, std::string>>& shard : shards_) {
            total += shard->Size();
        }
        return total;
    }

private:
    kvstore::SkipList<std::string, std::string>& ShardFor(const std::string& key) {
        return *shards_[ShardIndex(key)];
    }

    const kvstore::SkipList<std::string, std::string>& ShardFor(const std::string& key) const {
        return *shards_[ShardIndex(key)];
    }

    std::size_t ShardIndex(const std::string& key) const {
        return hasher_(key) % shards_.size();
    }

    std::vector<std::unique_ptr<kvstore::SkipList<std::string, std::string>>> shards_;
    std::hash<std::string> hasher_;
};

std::string TempWalPath(const std::string& name) {
    return "/tmp/kvstore-bench/" + name;
}

void RemoveFileIfExists(const std::string& path) {
    static_cast<void>(std::remove(path.c_str()));
}

WorkloadMode ParseWorkloadMode(const std::string& text) {
    if (text == "mixed") {
        return WorkloadMode::kMixed;
    }
    if (text == "read") {
        return WorkloadMode::kReadHeavy;
    }
    if (text == "write") {
        return WorkloadMode::kWriteHeavy;
    }
    throw std::invalid_argument("invalid workload mode: " + text);
}

const char* WorkloadModeName(WorkloadMode mode) {
    switch (mode) {
    case WorkloadMode::kMixed:
        return "mixed";
    case WorkloadMode::kReadHeavy:
        return "read";
    case WorkloadMode::kWriteHeavy:
        return "write";
    }
    return "unknown";
}

const char* WorkloadModeDescription(WorkloadMode mode) {
    switch (mode) {
    case WorkloadMode::kMixed:
        return "40% PUT, 50% GET, 10% DELETE/PUT+GET";
    case WorkloadMode::kReadHeavy:
        return "90% GET, 10% PUT";
    case WorkloadMode::kWriteHeavy:
        return "80% PUT, 10% GET, 10% DELETE";
    }
    return "";
}

template <typename Store>
void PreloadStore(Store* store, int preload_keys) {
    for (int index = 0; index < preload_keys; ++index) {
        static_cast<void>(store->Put("seed-key-" + std::to_string(index),
                                     "seed-value-" + std::to_string(index)));
    }
}

template <typename Store>
BenchmarkResult RunBenchmark(const std::string& name,
                             Store* store,
                             int thread_count,
                             int operations_per_thread,
                             int preload_keys,
                             WorkloadMode workload_mode,
                             std::size_t (*size_fn)(Store*)) {
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));

    const auto start = std::chrono::steady_clock::now();
    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([=]() {
            std::string value;
            for (int operation = 0; operation < operations_per_thread; ++operation) {
                const int global_index = (thread_index * operations_per_thread) + operation;
                const int preload_index =
                    preload_keys == 0 ? 0 : (global_index % preload_keys);
                const std::string read_key = "seed-key-" + std::to_string(preload_index);
                const std::string write_key = "key-" + std::to_string(global_index);
                const std::string payload = "value-" + std::to_string(operation);

                if (workload_mode == WorkloadMode::kReadHeavy) {
                    if ((operation % 10) == 0) {
                        static_cast<void>(store->Put(write_key, payload));
                    } else {
                        static_cast<void>(store->Get(read_key, &value));
                    }
                    continue;
                }

                if (workload_mode == WorkloadMode::kWriteHeavy) {
                    switch (operation % 10) {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                        static_cast<void>(store->Put(write_key, payload));
                        break;
                    case 8:
                        static_cast<void>(store->Get(read_key, &value));
                        break;
                    case 9:
                        static_cast<void>(store->Delete(read_key));
                        break;
                    default:
                        break;
                    }
                    continue;
                }

                switch (operation % 10) {
                case 0:
                case 1:
                case 2:
                    static_cast<void>(store->Put(write_key, payload));
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    static_cast<void>(store->Get(read_key, &value));
                    break;
                case 8:
                    static_cast<void>(store->Delete(read_key));
                    break;
                case 9:
                    static_cast<void>(store->Put(write_key, payload + "-update"));
                    static_cast<void>(store->Get(read_key, &value));
                    break;
                default:
                    break;
                }
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
    const auto end = std::chrono::steady_clock::now();

    const long long total_operations =
        static_cast<long long>(thread_count) * static_cast<long long>(operations_per_thread);
    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const double seconds = static_cast<double>(elapsed_ns) / 1000000000.0;

    BenchmarkResult result;
    result.name = name;
    result.thread_count = thread_count;
    result.operations_per_thread = operations_per_thread;
    result.total_operations = total_operations;
    result.seconds = seconds;
    result.throughput_ops = static_cast<double>(total_operations) / seconds;
    result.average_latency_ns = static_cast<double>(elapsed_ns) /
                                static_cast<double>(total_operations);
    result.final_size = size_fn(store);
    return result;
}

std::size_t KVStoreSize(kvstore::KVStore* store) {
    return store->Scan("", "~").size();
}

std::size_t MapStoreSize(MapStore* store) {
    return store->Size();
}

std::size_t ShardedMapStoreSize(ShardedMapStore* store) {
    return store->Size();
}

std::size_t ShardedSkipListStoreSize(ShardedSkipListStore* store) {
    return store->Size();
}

void PrintHeader() {
    std::cout << std::left
              << std::setw(22) << "benchmark"
              << std::setw(10) << "threads"
              << std::setw(14) << "ops/thread"
              << std::setw(14) << "total_ops"
              << std::setw(12) << "seconds"
              << std::setw(18) << "throughput(op/s)"
              << std::setw(18) << "avg_latency(ns)"
              << std::setw(12) << "final_size"
              << '\n';
}

void PrintResult(const BenchmarkResult& result) {
    std::cout << std::left
              << std::setw(22) << result.name
              << std::setw(10) << result.thread_count
              << std::setw(14) << result.operations_per_thread
              << std::setw(14) << result.total_operations
              << std::setw(12) << std::fixed << std::setprecision(4) << result.seconds
              << std::setw(18) << std::fixed << std::setprecision(2) << result.throughput_ops
              << std::setw(18) << std::fixed << std::setprecision(2)
              << result.average_latency_ns
              << std::setw(12) << result.final_size
              << '\n';
}

void PrintUsage() {
    std::cerr << "Usage: ./bin/kvstore_compare_bench [ops_per_thread] [max_threads]"
                 " [preload_keys>=0] [mixed|read|write]\n"
                 "  ops_per_thread: operations executed by each thread (default 20000)\n"
                 "  max_threads: maximum thread count in 1/2/4/... sweep (default 8)\n"
                 "  preload_keys: keys inserted before the benchmark starts (default 0)\n"
                 "  workload: mixed | read | write (default mixed)\n";
}

int ParsePositiveInt(const char* text, const char* field_name) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(std::string("invalid ") + field_name);
    }
    return static_cast<int>(value);
}

int ParseNonNegativeInt(const char* text, const char* field_name) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(std::string("invalid ") + field_name);
    }
    return static_cast<int>(value);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            PrintUsage();
            return 0;
        }
    }

    int operations_per_thread = 20000;
    int max_threads = 8;
    int preload_keys = 0;
    WorkloadMode workload_mode = WorkloadMode::kMixed;

    try {
        operations_per_thread = argc > 1 ? ParsePositiveInt(argv[1], "ops per thread") : 20000;
        max_threads = argc > 2 ? ParsePositiveInt(argv[2], "max threads") : 8;
        preload_keys = argc > 3 ? ParseNonNegativeInt(argv[3], "preload keys") : 0;
        workload_mode = argc > 4 ? ParseWorkloadMode(argv[4]) : WorkloadMode::kMixed;
    } catch (const std::invalid_argument& ex) {
        PrintUsage();
        std::cerr << "Compare benchmark error: " << ex.what() << '\n';
        return 1;
    }

    std::vector<int> thread_counts;
    for (int threads = 1; threads <= max_threads; threads *= 2) {
        thread_counts.push_back(threads);
    }
    if (thread_counts.back() != max_threads) {
        thread_counts.push_back(max_threads);
    }

    std::cout << "Workload " << WorkloadModeName(workload_mode)
              << ": " << WorkloadModeDescription(workload_mode) << '\n';
    std::cout << "Preloaded keys: " << preload_keys << '\n';
    PrintHeader();

    for (const int thread_count : thread_counts) {
        {
            kvstore::EngineOptions options;
            options.enable_wal = false;
            options.wal_path = TempWalPath("compare-memory-" + std::to_string(thread_count) + ".log");
            RemoveFileIfExists(options.wal_path);
            kvstore::KVStore store(options);
            PreloadStore(&store, preload_keys);
            PrintResult(RunBenchmark("kvstore_no_wal",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     preload_keys,
                                     workload_mode,
                                     &KVStoreSize));
            RemoveFileIfExists(options.wal_path);
        }

        {
            kvstore::EngineOptions options;
            options.enable_wal = true;
            options.wal_sync_interval_ms = 10;
            options.wal_path = TempWalPath("compare-wal-" + std::to_string(thread_count) + ".log");
            RemoveFileIfExists(options.wal_path);
            kvstore::KVStore store(options);
            PreloadStore(&store, preload_keys);
            PrintResult(RunBenchmark("kvstore_with_wal",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     preload_keys,
                                     workload_mode,
                                     &KVStoreSize));
            RemoveFileIfExists(options.wal_path);
        }

        {
            MapStore store;
            PreloadStore(&store, preload_keys);
            PrintResult(RunBenchmark("std_map_mutex",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     preload_keys,
                                     workload_mode,
                                     &MapStoreSize));
        }

        {
            ShardedMapStore store;
            PreloadStore(&store, preload_keys);
            PrintResult(RunBenchmark("std_map_sharded",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     preload_keys,
                                     workload_mode,
                                     &ShardedMapStoreSize));
        }

        {
            ShardedSkipListStore store;
            PreloadStore(&store, preload_keys);
            PrintResult(RunBenchmark("skiplist_sharded",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     preload_keys,
                                     workload_mode,
                                     &ShardedSkipListStoreSize));
        }
    }

    return 0;
}
