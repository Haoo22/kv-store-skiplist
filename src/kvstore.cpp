#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"
#include "kvstore/WAL.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kvstore {

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

    SkipList<std::string, std::string> index;
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
