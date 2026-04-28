#include "kvstore/Protocol.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
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

bool TryParseSize(const std::string& text, std::size_t* value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    *value = static_cast<std::size_t>(parsed);
    return true;
}

}  // namespace

void RequestCodec::Append(const char* data, std::size_t size) {
    buffer_.append(data, size);
}

std::vector<DecodedRequest> RequestCodec::ExtractRequests() {
    std::vector<DecodedRequest> requests;

    while (!buffer_.empty()) {
        DecodedRequest request;
        if (!TryExtractResp(&buffer_, &request)) {
            break;
        }
        requests.push_back(std::move(request));
    }

    return requests;
}

const std::string& RequestCodec::buffer() const noexcept {
    return buffer_;
}

bool RequestCodec::TryExtractResp(std::string* buffer, DecodedRequest* request) {
    if (buffer->empty()) {
        return false;
    }
    if (buffer->front() != '*') {
        throw std::runtime_error("RESP request must start with '*'");
    }

    const std::string::size_type array_line_end = buffer->find("\r\n");
    if (array_line_end == std::string::npos) {
        return false;
    }

    std::size_t token_count = 0;
    if (!TryParseSize(buffer->substr(1, array_line_end - 1), &token_count)) {
        throw std::runtime_error("invalid RESP array length");
    }

    std::string::size_type cursor = array_line_end + 2;
    std::vector<std::string> tokens;
    tokens.reserve(token_count);

    for (std::size_t index = 0; index < token_count; ++index) {
        if (cursor >= buffer->size()) {
            return false;
        }
        if ((*buffer)[cursor] != '$') {
            throw std::runtime_error("invalid RESP bulk string prefix");
        }

        const std::string::size_type size_line_end = buffer->find("\r\n", cursor);
        if (size_line_end == std::string::npos) {
            return false;
        }

        std::size_t token_size = 0;
        if (!TryParseSize(buffer->substr(cursor + 1, size_line_end - cursor - 1), &token_size)) {
            throw std::runtime_error("invalid RESP bulk string length");
        }

        const std::string::size_type data_begin = size_line_end + 2;
        const std::string::size_type data_end = data_begin + token_size;
        if (data_end + 2 > buffer->size()) {
            return false;
        }
        if (buffer->compare(data_end, 2, "\r\n") != 0) {
            throw std::runtime_error("invalid RESP bulk string terminator");
        }

        tokens.push_back(buffer->substr(data_begin, token_size));
        cursor = data_end + 2;
    }

    request->tokens = std::move(tokens);
    buffer->erase(0, cursor);
    return true;
}

CommandProcessor::CommandProcessor(KVStore& store)
    : store_(store) {}

std::string CommandProcessor::Execute(const std::string& line) const {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return "ERROR empty command\r\n";
    }

    // 命令字统一按大小写不敏感处理，参数仍保留原样。
    const std::string::size_type command_end = FindTokenEnd(trimmed, 0);
    const std::string command = ToUpper(trimmed.substr(0, command_end));
    const std::string::size_type args_begin = SkipSpaces(trimmed, command_end);

    if (command == "PUT") {
        if (args_begin == trimmed.size()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        const std::string::size_type key_end = FindTokenEnd(trimmed, args_begin);
        const std::string::size_type value_begin = SkipSpaces(trimmed, key_end);
        if (value_begin == trimmed.size()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        return ExecuteCommand(command,
                              {trimmed.substr(args_begin, key_end - args_begin),
                               trimmed.substr(value_begin)});
    }

    std::vector<std::string> args;
    std::string::size_type cursor = args_begin;
    while (cursor < trimmed.size()) {
        const std::string::size_type token_end = FindTokenEnd(trimmed, cursor);
        args.push_back(trimmed.substr(cursor, token_end - cursor));
        cursor = SkipSpaces(trimmed, token_end);
    }
    return ExecuteCommand(command, args);
}

std::string CommandProcessor::Execute(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) {
        return "ERROR empty command\r\n";
    }

    const std::string command = ToUpper(tokens.front());
    return ExecuteCommand(command,
                          std::vector<std::string>(tokens.begin() + 1, tokens.end()));
}

std::string CommandProcessor::ExecuteCommand(const std::string& command,
                                             const std::vector<std::string>& args) const {
    if (command == "PING") {
        return args.empty() ? "PONG\r\n" : "ERROR usage: PING\r\n";
    }

    if (command == "GET") {
        if (args.size() != 1) {
            return "ERROR usage: GET <key>\r\n";
        }

        std::string value;
        if (!store_.Get(args[0], &value)) {
            return "NOT_FOUND\r\n";
        }
        return "VALUE " + value + "\r\n";
    }

    if (command == "DEL") {
        if (args.size() != 1) {
            return "ERROR usage: DEL <key>\r\n";
        }
        return store_.Delete(args[0]) ? "OK DELETE\r\n" : "NOT_FOUND\r\n";
    }

    if (command == "SCAN") {
        if (args.size() != 2) {
            return "ERROR usage: SCAN <start> <end>\r\n";
        }

        const auto pairs = store_.Scan(args[0], args[1]);
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
        return args.empty() ? "BYE\r\n" : "ERROR usage: QUIT\r\n";
    }

    if (command == "CHECKPOINT") {
        if (!args.empty()) {
            return "ERROR usage: CHECKPOINT\r\n";
        }
        try {
            return store_.Checkpoint() ? "OK CHECKPOINT\r\n"
                                       : "ERROR checkpoint unavailable\r\n";
        } catch (const std::exception&) {
            return "ERROR checkpoint failed\r\n";
        }
    }

    if (command == "PUT") {
        if (args.size() != 2 || args[0].empty() || args[1].empty()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }
        return store_.Put(args[0], args[1]) ? "OK PUT\r\n" : "OK UPDATE\r\n";
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
