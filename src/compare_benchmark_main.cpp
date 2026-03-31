#include "kvstore/kvstore.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

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

std::string TempWalPath(const std::string& name) {
    return "/tmp/kvstore-bench/" + name;
}

void RemoveFileIfExists(const std::string& path) {
    static_cast<void>(std::remove(path.c_str()));
}

template <typename Store>
BenchmarkResult RunBenchmark(const std::string& name,
                             Store* store,
                             int thread_count,
                             int operations_per_thread,
                             std::size_t (*size_fn)(Store*)) {
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));

    const auto start = std::chrono::steady_clock::now();
    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([=]() {
            std::string value;
            for (int operation = 0; operation < operations_per_thread; ++operation) {
                const std::string key =
                    "key-" + std::to_string((thread_index * operations_per_thread) + operation);
                const std::string payload = "value-" + std::to_string(operation);

                switch (operation % 10) {
                case 0:
                case 1:
                case 2:
                    static_cast<void>(store->Put(key, payload));
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    static_cast<void>(store->Get(key, &value));
                    break;
                case 8:
                    static_cast<void>(store->Delete(key));
                    break;
                case 9:
                    static_cast<void>(store->Put(key, payload + "-update"));
                    static_cast<void>(store->Get(key, &value));
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

}  // namespace

int main(int argc, char** argv) {
    const int operations_per_thread = argc > 1 ? std::stoi(argv[1]) : 20000;
    const int max_threads = argc > 2 ? std::stoi(argv[2]) : 8;

    if (operations_per_thread <= 0 || max_threads <= 0) {
        std::cerr << "Usage: ./bin/kvstore_compare_bench [ops_per_thread] [max_threads]\n";
        return 1;
    }

    std::vector<int> thread_counts;
    for (int threads = 1; threads <= max_threads; threads *= 2) {
        thread_counts.push_back(threads);
    }
    if (thread_counts.back() != max_threads) {
        thread_counts.push_back(max_threads);
    }

    std::cout << "Mixed workload: 40% PUT, 50% GET, 10% DELETE/PUT+GET\n";
    PrintHeader();

    for (const int thread_count : thread_counts) {
        {
            kvstore::EngineOptions options;
            options.enable_wal = false;
            options.wal_path = TempWalPath("compare-memory-" + std::to_string(thread_count) + ".log");
            RemoveFileIfExists(options.wal_path);
            kvstore::KVStore store(options);
            PrintResult(RunBenchmark("kvstore_no_wal",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     &KVStoreSize));
            RemoveFileIfExists(options.wal_path);
        }

        {
            kvstore::EngineOptions options;
            options.enable_wal = true;
            options.wal_path = TempWalPath("compare-wal-" + std::to_string(thread_count) + ".log");
            RemoveFileIfExists(options.wal_path);
            kvstore::KVStore store(options);
            PrintResult(RunBenchmark("kvstore_with_wal",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     &KVStoreSize));
            RemoveFileIfExists(options.wal_path);
        }

        {
            MapStore store;
            PrintResult(RunBenchmark("std_map_mutex",
                                     &store,
                                     thread_count,
                                     operations_per_thread,
                                     &MapStoreSize));
        }
    }

    return 0;
}
