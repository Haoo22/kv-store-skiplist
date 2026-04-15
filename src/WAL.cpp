#include "kvstore/WAL.hpp"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace kvstore {
namespace {

constexpr std::uint32_t kRecordMagic = 0x4b56574cU;

struct RecordHeader {
    std::uint32_t magic;
    std::uint8_t type;
    std::uint32_t key_size;
    std::uint32_t value_size;
    std::uint32_t checksum;
};

class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}

    ~FileDescriptor() {
        Reset();
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            Reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int Get() const noexcept {
        return fd_;
    }

    void Reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

std::runtime_error MakeErrno(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

std::string ParentDirectory(const std::string& path) {
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

void EnsureDirectory(const std::string& directory) {
    if (directory.empty() || directory == ".") {
        return;
    }

    std::string current;
    if (directory.front() == '/') {
        current = "/";
    }

    std::size_t pos = 0;
    while (pos < directory.size()) {
        const std::size_t next = directory.find('/', pos);
        const std::string part = directory.substr(pos, next - pos);
        pos = (next == std::string::npos) ? directory.size() : next + 1;

        if (part.empty() || part == ".") {
            continue;
        }

        if (!current.empty() && current.back() != '/') {
            current.push_back('/');
        }
        current.append(part);

        if (::mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            throw MakeErrno("mkdir failed for " + current);
        }
    }
}

std::uint32_t ComputeChecksum(const LogRecord& record) {
    std::uint32_t hash = 2166136261u;
    auto update = [&hash](const unsigned char* data, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= static_cast<std::uint32_t>(data[index]);
            hash *= 16777619u;
        }
    };

    const auto type = static_cast<std::uint8_t>(record.type);
    const auto key_size = static_cast<std::uint32_t>(record.key.size());
    const auto value_size = static_cast<std::uint32_t>(record.value.size());

    update(reinterpret_cast<const unsigned char*>(&type), sizeof(type));
    update(reinterpret_cast<const unsigned char*>(&key_size), sizeof(key_size));
    update(reinterpret_cast<const unsigned char*>(&value_size), sizeof(value_size));
    update(reinterpret_cast<const unsigned char*>(record.key.data()), record.key.size());
    update(reinterpret_cast<const unsigned char*>(record.value.data()), record.value.size());
    return hash;
}

void WriteAll(int fd, const void* data, std::size_t length) {
    const char* buffer = static_cast<const char*>(data);
    std::size_t written = 0;

    while (written < length) {
        const ssize_t result = ::write(fd, buffer + written, length - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw MakeErrno("write failed");
        }
        written += static_cast<std::size_t>(result);
    }
}

bool ReadAll(int fd, void* data, std::size_t length, std::size_t* bytes_read) {
    char* buffer = static_cast<char*>(data);
    std::size_t total = 0;

    while (total < length) {
        const ssize_t result = ::read(fd, buffer + total, length - total);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw MakeErrno("read failed");
        }
        if (result == 0) {
            break;
        }
        total += static_cast<std::size_t>(result);
    }

    if (bytes_read != nullptr) {
        *bytes_read = total;
    }
    return total == length;
}

}  // namespace

class WAL::Impl {
public:
    explicit Impl(const std::string& file_path)
        : fd_(OpenAppendFd(file_path)) {}

    void Append(const LogRecord& record) {
        const RecordHeader header {
            kRecordMagic,
            static_cast<std::uint8_t>(record.type),
            static_cast<std::uint32_t>(record.key.size()),
            static_cast<std::uint32_t>(record.value.size()),
            ComputeChecksum(record),
        };

        thread_local std::vector<char> buffer;
        buffer.resize(sizeof(header) + record.key.size() + record.value.size());
        char* cursor = buffer.data();
        std::memcpy(cursor, &header, sizeof(header));
        cursor += sizeof(header);
        if (!record.key.empty()) {
            std::memcpy(cursor, record.key.data(), record.key.size());
            cursor += record.key.size();
        }
        if (!record.value.empty()) {
            std::memcpy(cursor, record.value.data(), record.value.size());
        }

        WriteAll(fd_.Get(), buffer.data(), buffer.size());
    }

    ReplayStats Replay(const std::string& file_path,
                       const std::function<void(const LogRecord&)>& apply) const {
        FileDescriptor reader(OpenReadFd(file_path));
        ReplayStats stats;

        while (true) {
            RecordHeader header {};
            std::size_t header_bytes = 0;
            if (!ReadAll(reader.Get(), &header, sizeof(header), &header_bytes)) {
                stats.skipped_tail_bytes += header_bytes;
                break;
            }

            if (header.magic != kRecordMagic) {
                throw std::runtime_error("WAL replay failed: invalid record magic");
            }

            if (header.type != static_cast<std::uint8_t>(RecordType::kPut) &&
                header.type != static_cast<std::uint8_t>(RecordType::kDelete)) {
                throw std::runtime_error("WAL replay failed: unknown record type");
            }

            LogRecord record;
            record.type = static_cast<RecordType>(header.type);
            record.key.resize(header.key_size);
            record.value.resize(header.value_size);

            std::size_t payload_bytes = 0;
            if (!record.key.empty()) {
                if (!ReadAll(reader.Get(), &record.key[0], record.key.size(), &payload_bytes)) {
                    stats.skipped_tail_bytes += sizeof(header) + payload_bytes;
                    break;
                }
            }

            std::size_t value_bytes = 0;
            if (!record.value.empty()) {
                if (!ReadAll(reader.Get(), &record.value[0], record.value.size(), &value_bytes)) {
                    stats.skipped_tail_bytes += sizeof(header) + record.key.size() + value_bytes;
                    break;
                }
            }

            if (ComputeChecksum(record) != header.checksum) {
                throw std::runtime_error("WAL replay failed: checksum mismatch");
            }

            apply(record);
            ++stats.applied_records;
        }

        return stats;
    }

    void Sync() {
        if (::fsync(fd_.Get()) != 0) {
            throw MakeErrno("fsync failed");
        }
    }

private:
    static int OpenAppendFd(const std::string& file_path) {
        EnsureDirectory(ParentDirectory(file_path));
        const int fd = ::open(file_path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (fd < 0) {
            throw MakeErrno("open WAL for append failed");
        }
        return fd;
    }

    static int OpenReadFd(const std::string& file_path) {
        const int fd = ::open(file_path.c_str(), O_RDONLY);
        if (fd < 0) {
            if (errno == ENOENT) {
                return ::open("/dev/null", O_RDONLY);
            }
            throw MakeErrno("open WAL for replay failed");
        }
        return fd;
    }

    FileDescriptor fd_;
};

WAL::WAL(std::string file_path, int sync_interval_ms)
    : file_path_(std::move(file_path)),
      sync_interval_ms_(sync_interval_ms < 0 ? 0 : sync_interval_ms),
      impl_(new Impl(file_path_)) {
    StartSyncThread();
}

WAL::~WAL() {
    StopSyncThread();
    std::lock_guard<std::mutex> lock(append_mutex_);
    impl_->Sync();
}

void WAL::AppendPut(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(append_mutex_);
        impl_->Append(LogRecord {RecordType::kPut, key, value});
    }
    MarkDirty();
}

void WAL::AppendDelete(const std::string& key) {
    {
        std::lock_guard<std::mutex> lock(append_mutex_);
        impl_->Append(LogRecord {RecordType::kDelete, key, ""});
    }
    MarkDirty();
}

ReplayStats WAL::Replay(const std::function<void(const LogRecord&)>& apply) const {
    return impl_->Replay(file_path_, apply);
}

void WAL::Sync() {
    impl_->Sync();
}

const std::string& WAL::path() const noexcept {
    return file_path_;
}

void WAL::StartSyncThread() {
    if (sync_interval_ms_ == 0) {
        return;
    }

    sync_thread_ = std::thread([this]() {
        SyncLoop();
    });
}

void WAL::StopSyncThread() {
    if (!sync_thread_.joinable()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        stop_sync_ = true;
    }
    sync_cv_.notify_one();
    sync_thread_.join();
}

void WAL::MarkDirty() {
    if (sync_interval_ms_ == 0) {
        impl_->Sync();
        return;
    }

    std::lock_guard<std::mutex> lock(sync_mutex_);
    dirty_ = true;
}

void WAL::SyncLoop() {
    const auto interval = std::chrono::milliseconds(sync_interval_ms_);

    while (true) {
        {
            std::unique_lock<std::mutex> lock(sync_mutex_);
            sync_cv_.wait_for(lock, interval, [this]() {
                return stop_sync_;
            });
        }

        bool should_stop = false;
        bool should_sync = false;
        {
            std::lock_guard<std::mutex> lock(sync_mutex_);
            should_stop = stop_sync_;
            should_sync = dirty_;
            dirty_ = false;
        }

        if (should_sync) {
            impl_->Sync();
        }

        if (should_stop) {
            return;
        }
    }
}

}  // namespace kvstore
