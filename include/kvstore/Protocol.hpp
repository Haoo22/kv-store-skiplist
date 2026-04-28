#pragma once

#include "kvstore/kvstore.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace kvstore {

struct DecodedRequest {
    std::vector<std::string> tokens;
};

// 面向连接的协议编解码器。
// 仅支持 RESP-like 长度前缀请求。
class RequestCodec {
public:
    void Append(const char* data, std::size_t size);
    std::vector<DecodedRequest> ExtractRequests();
    const std::string& buffer() const noexcept;

private:
    static bool TryExtractResp(std::string* buffer, DecodedRequest* request);

    std::string buffer_;
};

// 将文本命令映射为 KVStore 操作并生成协议响应。
class CommandProcessor {
public:
    explicit CommandProcessor(KVStore& store);

    std::string Execute(const std::string& line) const;
    std::string Execute(const std::vector<std::string>& tokens) const;

private:
    std::string ExecuteCommand(const std::string& command,
                               const std::vector<std::string>& args) const;
    static std::string Trim(const std::string& text);

    KVStore& store_;
};

}  // namespace kvstore
