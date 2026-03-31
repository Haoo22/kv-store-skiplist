#pragma once

#include <memory>
#include <string>

namespace kvstore {

struct EngineOptions {
    std::string wal_path {"data/wal.log"};
};

class KVStore {
public:
    explicit KVStore(EngineOptions options);
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) noexcept = default;
    KVStore& operator=(KVStore&&) noexcept = default;

    const EngineOptions& options() const noexcept;

private:
    class Impl;

    EngineOptions options_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
