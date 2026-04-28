#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kvstore {

// 存储引擎运行参数。
struct EngineOptions {
    std::string wal_path {"data/wal.log"};
    bool enable_wal {true};
    int wal_sync_interval_ms {0};
};

// KVStore 对外提供统一的键值存取接口。
// 内部实现由跳表内存索引和可选 WAL 持久化组成。
class KVStore {
public:
    explicit KVStore(EngineOptions options);
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) noexcept = default;
    KVStore& operator=(KVStore&&) noexcept = default;

    // 写入或更新键值；返回 true 表示新插入，false 表示覆盖旧值。
    bool Put(const std::string& key, const std::string& value);
    // 读取键值；未命中时返回 false。
    bool Get(const std::string& key, std::string* value) const;
    // 删除键；键不存在时返回 false。
    bool Delete(const std::string& key);
    // 返回 [start, end] 闭区间内按 key 有序排列的键值对。
    std::vector<std::pair<std::string, std::string>> Scan(
        const std::string& start,
        const std::string& end) const;
    // 将当前内存状态落成快照，并重置 WAL。
    // 当 WAL 未启用时返回 false。
    bool Checkpoint();

    const EngineOptions& options() const noexcept;

private:
    class Impl;

    EngineOptions options_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
