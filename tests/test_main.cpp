#include "kvstore/kvstore.hpp"
#include "kvstore/Protocol.hpp"
#include "kvstore/SkipList.hpp"
#include "kvstore/WAL.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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
    const std::string directory =
        "/tmp/kvstore-tests-" + std::to_string(static_cast<long long>(::getpid()));
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

    // 基础功能测试：跳表、引擎增删查扫与 WAL 恢复。
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
    Ensure(store.Put("aardvark", "0"), "engine insert aardvark");
    Ensure(!store.Put("alpha", "3"), "engine update alpha");

    std::string engine_value;
    Ensure(store.Get("alpha", &engine_value), "engine get alpha");
    Ensure(engine_value == "3", "engine updated alpha value");
    Ensure(store.Delete("beta"), "engine delete beta");
    Ensure(!store.Delete("missing"), "delete missing engine key");

    const auto ordered_range = store.Scan("a", "z");
    Ensure(ordered_range.size() == 2, "engine range size after delete");
    Ensure(ordered_range[0].first == "aardvark", "engine range should stay globally ordered");
    Ensure(ordered_range[0].second == "0", "engine first range value");
    Ensure(ordered_range[1].first == "alpha", "engine second range key");
    Ensure(ordered_range[1].second == "3", "engine second range value");

    {
        kvstore::KVStore recovered_store(options);
        const auto recovered_range = recovered_store.Scan("a", "z");
        Ensure(recovered_range.size() == 2, "recovered range size");
        Ensure(recovered_range[0].first == "aardvark", "recovered range key 0");
        Ensure(recovered_range[0].second == "0", "recovered range value 0");
        Ensure(recovered_store.Get("alpha", &engine_value), "replay should recover alpha");
        Ensure(engine_value == "3", "recovered alpha value");
        Ensure(!recovered_store.Get("beta", &engine_value), "replay should preserve delete");
        Ensure(recovered_range[1].first == "alpha", "recovered range key 1");
        Ensure(recovered_range[1].second == "3", "recovered range value 1");
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

    {
        // 协议测试：同时覆盖拆包、命令执行与响应格式。
        kvstore::EngineOptions protocol_options;
        protocol_options.wal_path = MakeWalPath("protocol.log");
        RemoveFileIfExists(protocol_options.wal_path);
        kvstore::KVStore protocol_store(protocol_options);
        kvstore::CommandProcessor processor(protocol_store);
        kvstore::LineCodec codec;

        static const char kChunk1[] = "PUT user alice\r\nGET ";
        codec.Append(kChunk1, sizeof(kChunk1) - 1);
        const auto first_lines = codec.ExtractLines();
        Ensure(first_lines.size() == 1, "codec should extract one complete line");
        Ensure(first_lines[0] == "PUT user alice", "codec first line content");
        Ensure(codec.buffer() == "GET ", "codec should retain partial command");

        static const char kChunk2[] = "user\r\nSCAN a z\r\nDEL user\r\nQUIT\r\n";
        codec.Append(kChunk2, sizeof(kChunk2) - 1);
        const auto second_lines = codec.ExtractLines();
        Ensure(second_lines.size() == 4, "codec should extract remaining lines");

        Ensure(processor.Execute(first_lines[0]) == "OK PUT\r\n", "protocol PUT response");
        Ensure(processor.Execute(second_lines[0]) == "VALUE alice\r\n", "protocol GET response");
        Ensure(processor.Execute(second_lines[1]) == "RESULT 1 user=alice\r\n",
               "protocol SCAN response");
        Ensure(processor.Execute(second_lines[2]) == "OK DELETE\r\n", "protocol DEL response");
        Ensure(processor.Execute(second_lines[3]) == "BYE\r\n", "protocol QUIT response");
        Ensure(processor.Execute("PING") == "PONG\r\n", "protocol PING response");
        Ensure(processor.Execute("GET missing") == "NOT_FOUND\r\n", "protocol missing GET");
        Ensure(processor.Execute("NOOP") == "ERROR unknown command\r\n",
               "protocol unknown command");

        RemoveFileIfExists(protocol_options.wal_path);
    }

    {
        // 并发写、读、删联合测试，验证跳表在多线程下的可见性与计数。
        kvstore::SkipList<int, std::string> concurrent_skip_list;
        std::vector<std::thread> writers;
        for (int thread_index = 0; thread_index < 4; ++thread_index) {
            writers.emplace_back([thread_index, &concurrent_skip_list]() {
                for (int offset = 0; offset < 200; ++offset) {
                    const int key = (thread_index * 1000) + offset;
                    static_cast<void>(concurrent_skip_list.Put(
                        key,
                        "value-" + std::to_string(thread_index) + "-" + std::to_string(offset)));
                }
            });
        }
        for (std::thread& writer : writers) {
            writer.join();
        }

        Ensure(concurrent_skip_list.Size() == 800, "concurrent inserts should preserve all keys");

        std::vector<std::thread> readers;
        for (int thread_index = 0; thread_index < 4; ++thread_index) {
            readers.emplace_back([thread_index, &concurrent_skip_list]() {
                for (int offset = 0; offset < 200; ++offset) {
                    const int key = (thread_index * 1000) + offset;
                    std::string read_value;
                    Ensure(concurrent_skip_list.Get(key, &read_value),
                           "concurrent get should find inserted key");
                }
            });
        }
        for (std::thread& reader : readers) {
            reader.join();
        }

        std::vector<std::thread> deleters;
        for (int thread_index = 0; thread_index < 4; ++thread_index) {
            deleters.emplace_back([thread_index, &concurrent_skip_list]() {
                for (int offset = 0; offset < 100; ++offset) {
                    const int key = (thread_index * 1000) + offset;
                    Ensure(concurrent_skip_list.Delete(key),
                           "concurrent delete should remove existing key");
                }
            });
        }
        for (std::thread& deleter : deleters) {
            deleter.join();
        }

        Ensure(concurrent_skip_list.Size() == 400, "concurrent deletes should update size");
        const auto concurrent_range = concurrent_skip_list.Scan(0, 5000);
        Ensure(concurrent_range.size() == 400, "range after concurrent delete should match size");
        for (std::size_t index = 1; index < concurrent_range.size(); ++index) {
            Ensure(concurrent_range[index - 1].first < concurrent_range[index].first,
                   "range after concurrent operations should remain ordered");
        }
    }

    {
        // 写入与全量扫描并发进行，检查扫描结果始终保持有序。
        kvstore::SkipList<int, std::string> scan_stress_skip_list;
        for (int key = 0; key < 2000; ++key) {
            static_cast<void>(scan_stress_skip_list.Put(key, "seed-" + std::to_string(key)));
        }

        std::thread writer([&scan_stress_skip_list]() {
            for (int round = 0; round < 20; ++round) {
                for (int key = 0; key < 2000; ++key) {
                    static_cast<void>(scan_stress_skip_list.Put(
                        key,
                        "updated-" + std::to_string(round) + "-" + std::to_string(key)));
                }
            }
        });

        std::thread scanner([&scan_stress_skip_list]() {
            for (int round = 0; round < 40; ++round) {
                const auto full_range = scan_stress_skip_list.Scan(0, 1999);
                for (std::size_t index = 1; index < full_range.size(); ++index) {
                    Ensure(full_range[index - 1].first < full_range[index].first,
                           "concurrent scan should remain ordered");
                }
            }
        });

        writer.join();
        scanner.join();

        Ensure(scan_stress_skip_list.Size() == 2000,
               "scan stress test should preserve final key count");
    }

    {
        kvstore::SkipList<int, std::string> consistent_scan_skip_list;
        for (int key = 0; key < 2000; ++key) {
            static_cast<void>(consistent_scan_skip_list.Put(
                key,
                "value-" + std::to_string(key)));
        }

        std::thread writer([&consistent_scan_skip_list]() {
            for (int round = 0; round < 10; ++round) {
                for (int key = 0; key < 1000; ++key) {
                    Ensure(consistent_scan_skip_list.Delete(key),
                           "writer should delete existing key during consistency test");
                }
                for (int key = 0; key < 1000; ++key) {
                    static_cast<void>(consistent_scan_skip_list.Put(
                        key,
                        "restored-" + std::to_string(round) + "-" + std::to_string(key)));
                }
            }
        });

        std::thread scanner([&consistent_scan_skip_list]() {
            for (int round = 0; round < 40; ++round) {
                const auto full_range = consistent_scan_skip_list.Scan(0, 1999);
                Ensure(full_range.size() >= 1000 && full_range.size() <= 2000,
                       "scan should observe a valid in-range key count");
                for (std::size_t index = 1; index < full_range.size(); ++index) {
                    Ensure(full_range[index - 1].first < full_range[index].first,
                           "consistent scan should remain ordered");
                }
            }
        });

        writer.join();
        scanner.join();

        Ensure(consistent_scan_skip_list.Size() == 2000,
               "consistency scan test should restore full key set");
    }

    {
        kvstore::SkipList<int, std::string> churn_skip_list;
        for (int round = 0; round < 20; ++round) {
            for (int key = 0; key < 1000; ++key) {
                static_cast<void>(churn_skip_list.Put(
                    key,
                    "round-" + std::to_string(round) + "-" + std::to_string(key)));
            }
            for (int key = 0; key < 1000; ++key) {
                Ensure(churn_skip_list.Delete(key),
                       "churn delete should remove previously inserted key");
            }
            Ensure(churn_skip_list.Empty(), "churn skip list should return to empty state");
        }

        for (int key = 0; key < 1000; ++key) {
            static_cast<void>(churn_skip_list.Put(key, "final-" + std::to_string(key)));
        }
        const auto churn_range = churn_skip_list.Scan(0, 999);
        Ensure(churn_range.size() == 1000, "churn test should preserve final inserts");
        for (std::size_t index = 1; index < churn_range.size(); ++index) {
            Ensure(churn_range[index - 1].first < churn_range[index].first,
                   "churn range should remain ordered after repeated reuse");
        }
    }

    RemoveFileIfExists(options.wal_path);
    RemoveFileIfExists(wal_path);

    std::cout << "kvstore WAL, protocol and skip list tests passed\n";
    return 0;
}
