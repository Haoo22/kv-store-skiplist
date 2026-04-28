#pragma once

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace kvstore {

// WAL 记录类型；当前只需要覆盖写入和删除。
enum class RecordType : std::uint8_t {
    kPut = 1,
    kDelete = 2,
};

// 一条逻辑日志记录的内存表示。
struct LogRecord {
    RecordType type {RecordType::kPut};
    std::string key;
    std::string value;
};

// 回放阶段的统计信息。
struct ReplayStats {
    std::size_t applied_records {0};
    std::size_t skipped_tail_bytes {0};
};

// 简单的追加式预写日志。
// 负责在写操作前记录变更，并在重启后回放恢复状态。
class WAL {
public:
    explicit WAL(std::string file_path, int sync_interval_ms = 0);
    ~WAL();

    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;
    WAL(WAL&&) noexcept = delete;
    WAL& operator=(WAL&&) noexcept = delete;

    // 追加写入一条 PUT 记录。
    void AppendPut(const std::string& key, const std::string& value);
    // 追加写入一条 DELETE 记录。
    void AppendDelete(const std::string& key);

    // 从日志头到尾回放完整记录；遇到截断尾部时忽略残缺数据。
    ReplayStats Replay(const std::function<void(const LogRecord&)>& apply) const;
    // 主动刷盘，确保已写入数据同步到磁盘。
    void Sync();

    const std::string& path() const noexcept;

private:
    void StartSyncThread();
    void StopSyncThread();
    void MarkDirty();
    void SyncLoop();

    class Impl;

    std::string file_path_;
    mutable std::mutex append_mutex_;
    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;
    bool stop_sync_ {false};
    bool dirty_ {false};
    int sync_interval_ms_ {0};
    std::thread sync_thread_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
