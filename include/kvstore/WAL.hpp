#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace kvstore {

enum class RecordType : std::uint8_t {
    kPut = 1,
    kDelete = 2,
};

struct LogRecord {
    RecordType type {RecordType::kPut};
    std::string key;
    std::string value;
};

struct ReplayStats {
    std::size_t applied_records {0};
    std::size_t skipped_tail_bytes {0};
};

class WAL {
public:
    explicit WAL(std::string file_path);
    ~WAL();

    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;
    WAL(WAL&&) noexcept;
    WAL& operator=(WAL&&) noexcept;

    void AppendPut(const std::string& key, const std::string& value);
    void AppendDelete(const std::string& key);

    ReplayStats Replay(const std::function<void(const LogRecord&)>& apply) const;
    void Sync();

    const std::string& path() const noexcept;

private:
    class Impl;

    std::string file_path_;
    mutable std::mutex append_mutex_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
