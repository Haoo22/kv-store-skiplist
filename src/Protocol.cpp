#include "kvstore/Protocol.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace kvstore {
namespace {

std::string ToUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

std::string::size_type SkipSpaces(const std::string& text, std::string::size_type pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    return pos;
}

std::string::size_type FindTokenEnd(const std::string& text, std::string::size_type pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) == 0) {
        ++pos;
    }
    return pos;
}

}  // namespace

void LineCodec::Append(const char* data, std::size_t size) {
    buffer_.append(data, size);
}

std::vector<std::string> LineCodec::ExtractLines() {
    std::vector<std::string> lines;
    std::string::size_type pos = 0;

    while ((pos = buffer_.find("\r\n")) != std::string::npos) {
        lines.emplace_back(buffer_.substr(0, pos));
        buffer_.erase(0, pos + 2);
    }

    return lines;
}

const std::string& LineCodec::buffer() const noexcept {
    return buffer_;
}

CommandProcessor::CommandProcessor(KVStore& store)
    : store_(store) {}

std::string CommandProcessor::Execute(const std::string& line) const {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return "ERROR empty command\r\n";
    }

    const std::string::size_type command_end = FindTokenEnd(trimmed, 0);
    const std::string command = ToUpper(trimmed.substr(0, command_end));
    const std::string::size_type args_begin = SkipSpaces(trimmed, command_end);

    if (command == "PING") {
        return args_begin == trimmed.size() ? "PONG\r\n" : "ERROR usage: PING\r\n";
    }

    if (command == "GET") {
        if (args_begin == trimmed.size()) {
            return "ERROR usage: GET <key>\r\n";
        }

        const std::string::size_type key_end = FindTokenEnd(trimmed, args_begin);
        if (SkipSpaces(trimmed, key_end) != trimmed.size()) {
            return "ERROR usage: GET <key>\r\n";
        }

        const std::string key = trimmed.substr(args_begin, key_end - args_begin);

        std::string value;
        if (!store_.Get(key, &value)) {
            return "NOT_FOUND\r\n";
        }
        return "VALUE " + value + "\r\n";
    }

    if (command == "DEL") {
        if (args_begin == trimmed.size()) {
            return "ERROR usage: DEL <key>\r\n";
        }

        const std::string::size_type key_end = FindTokenEnd(trimmed, args_begin);
        if (SkipSpaces(trimmed, key_end) != trimmed.size()) {
            return "ERROR usage: DEL <key>\r\n";
        }

        const std::string key = trimmed.substr(args_begin, key_end - args_begin);
        return store_.Delete(key) ? "OK DELETE\r\n" : "NOT_FOUND\r\n";
    }

    if (command == "SCAN") {
        if (args_begin == trimmed.size()) {
            return "ERROR usage: SCAN <start> <end>\r\n";
        }

        const std::string::size_type start_end = FindTokenEnd(trimmed, args_begin);
        const std::string::size_type end_begin = SkipSpaces(trimmed, start_end);
        if (end_begin == trimmed.size()) {
            return "ERROR usage: SCAN <start> <end>\r\n";
        }
        const std::string::size_type end_end = FindTokenEnd(trimmed, end_begin);
        if (SkipSpaces(trimmed, end_end) != trimmed.size()) {
            return "ERROR usage: SCAN <start> <end>\r\n";
        }

        const std::string start = trimmed.substr(args_begin, start_end - args_begin);
        const std::string end = trimmed.substr(end_begin, end_end - end_begin);
        const auto pairs = store_.Scan(start, end);
        std::string output = "RESULT " + std::to_string(pairs.size());
        for (const auto& pair : pairs) {
            output.push_back(' ');
            output.append(pair.first);
            output.push_back('=');
            output.append(pair.second);
        }
        output.append("\r\n");
        return output;
    }

    if (command == "QUIT") {
        return args_begin == trimmed.size() ? "BYE\r\n" : "ERROR usage: QUIT\r\n";
    }

    if (command == "PUT") {
        if (args_begin == trimmed.size()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        const std::string::size_type key_end = FindTokenEnd(trimmed, args_begin);
        if (key_end == std::string::npos) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }
        const std::string::size_type value_begin = SkipSpaces(trimmed, key_end);
        if (value_begin == trimmed.size()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        const std::string key = trimmed.substr(args_begin, key_end - args_begin);
        const std::string value = trimmed.substr(value_begin);
        if (key.empty() || value.empty()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        return store_.Put(key, value) ? "OK PUT\r\n" : "OK UPDATE\r\n";
    }

    return "ERROR unknown command\r\n";
}

std::string CommandProcessor::Trim(const std::string& text) {
    const auto is_space = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    const auto begin = std::find_if_not(text.begin(), text.end(), is_space);
    if (begin == text.end()) {
        return "";
    }

    const auto end = std::find_if_not(text.rbegin(), text.rend(), is_space).base();
    return std::string(begin, end);
}

}  // namespace kvstore
