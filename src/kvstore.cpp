#include "kvstore/kvstore.hpp"
#include "kvstore/SkipList.hpp"
#include "kvstore/WAL.hpp"

#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace kvstore {
namespace {

constexpr std::uint32_t kSnapshotMagic = 0x4b565353U;
constexpr std::uint32_t kSnapshotVersion = 1U;

struct SnapshotHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t record_count;
};

std::runtime_error MakeIoError(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

std::string SnapshotPathFromWalPath(const std::string& wal_path) {
    return wal_path + ".snapshot";
}

void WriteSnapshotFile(
    const std::string& snapshot_path,
    const std::vector<std::pair<std::string, std::string>>& entries) {
    const std::string temp_path = snapshot_path + ".tmp";
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("open snapshot for write failed: " + temp_path);
    }

    const SnapshotHeader header {
        kSnapshotMagic,
        kSnapshotVersion,
        static_cast<std::uint64_t>(entries.size()),
    };
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (const auto& entry : entries) {
        const std::uint32_t key_size = static_cast<std::uint32_t>(entry.first.size());
        const std::uint32_t value_size = static_cast<std::uint32_t>(entry.second.size());
        out.write(reinterpret_cast<const char*>(&key_size), sizeof(key_size));
        out.write(reinterpret_cast<const char*>(&value_size), sizeof(value_size));
        out.write(entry.first.data(), static_cast<std::streamsize>(entry.first.size()));
        out.write(entry.second.data(), static_cast<std::streamsize>(entry.second.size()));
    }

    if (!out.good()) {
        out.close();
        static_cast<void>(::unlink(temp_path.c_str()));
        throw std::runtime_error("write snapshot failed: " + temp_path);
    }
    out.close();

    if (::rename(temp_path.c_str(), snapshot_path.c_str()) != 0) {
        static_cast<void>(::unlink(temp_path.c_str()));
        throw MakeIoError("rename snapshot failed");
    }
}

void LoadSnapshotFile(
    const std::string& snapshot_path,
    SkipList<std::string, std::string>* index) {
    errno = 0;
    std::ifstream in(snapshot_path, std::ios::binary);
    if (!in.is_open()) {
        if (errno == ENOENT) {
            return;
        }
        throw std::runtime_error("open snapshot for read failed: " + snapshot_path);
    }

    SnapshotHeader header {};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in) {
        throw std::runtime_error("read snapshot header failed: " + snapshot_path);
    }
    if (header.magic != kSnapshotMagic || header.version != kSnapshotVersion) {
        throw std::runtime_error("invalid snapshot header: " + snapshot_path);
    }

    for (std::uint64_t record_index = 0; record_index < header.record_count; ++record_index) {
        std::uint32_t key_size = 0;
        std::uint32_t value_size = 0;
        in.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        in.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        if (!in) {
            throw std::runtime_error("read snapshot record header failed: " + snapshot_path);
        }

        std::string key(key_size, '\0');
        std::string value(value_size, '\0');
        if (key_size > 0) {
            in.read(&key[0], static_cast<std::streamsize>(key.size()));
        }
        if (value_size > 0) {
            in.read(&value[0], static_cast<std::streamsize>(value.size()));
        }
        if (!in) {
            throw std::runtime_error("read snapshot payload failed: " + snapshot_path);
        }

        static_cast<void>(index->Put(key, value));
    }
}

}  // namespace

class KVStore::Impl {
public:
    explicit Impl(const EngineOptions& options)
        : index(),
          wal(options.wal_path, options.wal_sync_interval_ms),
          wal_enabled(options.enable_wal),
          snapshot_path(SnapshotPathFromWalPath(options.wal_path)) {
        if (wal_enabled) {
            LoadSnapshotFile(snapshot_path, &index);
            // 启动阶段先加载快照，再回放增量 WAL。
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
    std::string snapshot_path;
};

KVStore::KVStore(EngineOptions options)
    : options_(std::move(options)), impl_(std::make_unique<Impl>(options_)) {}

KVStore::~KVStore() = default;

bool KVStore::Put(const std::string& key, const std::string& value) {
    if (impl_->wal_enabled) {
        // 先记 WAL，再更新内存索引，保证恢复时能重放这次写入。
        impl_->wal.AppendPut(key, value);
    }
    return impl_->index.Put(key, value);
}

bool KVStore::Get(const std::string& key, std::string* value) const {
    return impl_->index.Get(key, value);
}

bool KVStore::Delete(const std::string& key) {
    if (impl_->wal_enabled) {
        // 删除同样先落日志，避免进程异常退出后丢失删除意图。
        impl_->wal.AppendDelete(key);
    }
    return impl_->index.Delete(key);
}

std::vector<std::pair<std::string, std::string>> KVStore::Scan(
    const std::string& start,
    const std::string& end) const {
    return impl_->index.Scan(start, end);
}

bool KVStore::Checkpoint() {
    if (!impl_->wal_enabled) {
        return false;
    }

    const auto entries = impl_->index.Snapshot();
    WriteSnapshotFile(impl_->snapshot_path, entries);
    impl_->wal.Reset();
    return true;
}

const EngineOptions& KVStore::options() const noexcept {
    return options_;
}

}  // namespace kvstore
