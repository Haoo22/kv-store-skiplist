#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"
#include "kvstore/WAL.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kvstore {
namespace {

class ShardedSkipListIndex {
public:
    explicit ShardedSkipListIndex(std::size_t shard_count = 16)
        : shards_(shard_count == 0 ? 1 : shard_count) {
        for (std::unique_ptr<SkipList<std::string, std::string>>& shard : shards_) {
            shard = std::make_unique<SkipList<std::string, std::string>>();
        }
    }

    bool Put(const std::string& key, const std::string& value) {
        return ShardFor(key).Put(key, value);
    }

    bool Get(const std::string& key, std::string* value) const {
        return ShardFor(key).Get(key, value);
    }

    bool Delete(const std::string& key) {
        return ShardFor(key).Delete(key);
    }

    std::vector<std::pair<std::string, std::string>> Scan(const std::string& start,
                                                          const std::string& end) const {
        if (end < start) {
            return {};
        }

        std::vector<std::pair<std::string, std::string>> merged;
        for (const std::unique_ptr<SkipList<std::string, std::string>>& shard : shards_) {
            std::vector<std::pair<std::string, std::string>> partial = shard->Scan(start, end);
            merged.insert(merged.end(),
                          std::make_move_iterator(partial.begin()),
                          std::make_move_iterator(partial.end()));
        }

        std::sort(merged.begin(),
                  merged.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
        return merged;
    }

private:
    SkipList<std::string, std::string>& ShardFor(const std::string& key) {
        return *shards_[ShardIndex(key)];
    }

    const SkipList<std::string, std::string>& ShardFor(const std::string& key) const {
        return *shards_[ShardIndex(key)];
    }

    std::size_t ShardIndex(const std::string& key) const {
        return hasher_(key) % shards_.size();
    }

    std::vector<std::unique_ptr<SkipList<std::string, std::string>>> shards_;
    std::hash<std::string> hasher_;
};

}  // namespace

class KVStore::Impl {
public:
    explicit Impl(const EngineOptions& options)
        : index(),
          wal(options.wal_path, options.wal_sync_interval_ms),
          wal_enabled(options.enable_wal) {
        if (wal_enabled) {
            wal.Replay([this](const LogRecord& record) {
                if (record.type == RecordType::kPut) {
                    index.Put(record.key, record.value);
                    return;
                }
                index.Delete(record.key);
            });
        }
    }

    ~Impl() = default;

    ShardedSkipListIndex index;
    WAL wal;
    bool wal_enabled;
};

KVStore::KVStore(EngineOptions options)
    : options_(std::move(options)), impl_(std::make_unique<Impl>(options_)) {}

KVStore::~KVStore() = default;

bool KVStore::Put(const std::string& key, const std::string& value) {
    if (impl_->wal_enabled) {
        impl_->wal.AppendPut(key, value);
    }
    return impl_->index.Put(key, value);
}

bool KVStore::Get(const std::string& key, std::string* value) const {
    return impl_->index.Get(key, value);
}

bool KVStore::Delete(const std::string& key) {
    if (impl_->wal_enabled) {
        impl_->wal.AppendDelete(key);
    }
    return impl_->index.Delete(key);
}

std::vector<std::pair<std::string, std::string>> KVStore::Scan(
    const std::string& start,
    const std::string& end) const {
    return impl_->index.Scan(start, end);
}

const EngineOptions& KVStore::options() const noexcept {
    return options_;
}

}  // namespace kvstore
