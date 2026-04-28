#pragma once

#include "kvstore/kvstore.hpp"

#include <string>
#include <vector>

namespace kvstore {

// 面向连接的行协议编解码器。
// 负责累积网络分片，并按 CRLF 提取完整命令。
class LineCodec {
public:
    void Append(const char* data, std::size_t size);
    std::vector<std::string> ExtractLines();
    const std::string& buffer() const noexcept;

private:
    std::string buffer_;
};

// 将文本命令映射为 KVStore 操作并生成协议响应。
class CommandProcessor {
public:
    explicit CommandProcessor(KVStore& store);

    std::string Execute(const std::string& line) const;

private:
    static std::string Trim(const std::string& text);

    KVStore& store_;
};

}  // namespace kvstore
