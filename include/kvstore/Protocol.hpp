#pragma once

#include "kvstore/kvstore.hpp"

#include <string>
#include <vector>

namespace kvstore {

class LineCodec {
public:
    void Append(const char* data, std::size_t size);
    std::vector<std::string> ExtractLines();
    const std::string& buffer() const noexcept;

private:
    std::string buffer_;
};

class CommandProcessor {
public:
    explicit CommandProcessor(KVStore& store);

    std::string Execute(const std::string& line) const;

private:
    static std::vector<std::string> Tokenize(const std::string& line);
    static std::string Trim(const std::string& text);

    KVStore& store_;
};

}  // namespace kvstore
