#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace kvstore {

struct EngineOptions {
    std::string wal_path {"data/wal.log"};
    bool enable_wal {true};
};

class KVStore {
public:
    explicit KVStore(EngineOptions options);
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) noexcept = default;
    KVStore& operator=(KVStore&&) noexcept = default;

    bool Put(const std::string& key, const std::string& value);
    bool Get(const std::string& key, std::string* value) const;
    bool Delete(const std::string& key);
    std::vector<std::pair<std::string, std::string>> Scan(
        const std::string& start,
        const std::string& end) const;

    const EngineOptions& options() const noexcept;

private:
    class Impl;

    EngineOptions options_;
    mutable std::mutex mutex_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
