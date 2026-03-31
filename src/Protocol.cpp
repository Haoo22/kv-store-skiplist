#include "kvstore/Protocol.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
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

    const std::vector<std::string> tokens = Tokenize(trimmed);
    if (tokens.empty()) {
        return "ERROR empty command\r\n";
    }

    const std::string command = ToUpper(tokens[0]);

    if (command == "PING") {
        return "PONG\r\n";
    }

    if (command == "GET") {
        if (tokens.size() != 2) {
            return "ERROR usage: GET <key>\r\n";
        }

        std::string value;
        if (!store_.Get(tokens[1], &value)) {
            return "NOT_FOUND\r\n";
        }
        return "VALUE " + value + "\r\n";
    }

    if (command == "DEL") {
        if (tokens.size() != 2) {
            return "ERROR usage: DEL <key>\r\n";
        }
        return store_.Delete(tokens[1]) ? "OK DELETE\r\n" : "NOT_FOUND\r\n";
    }

    if (command == "SCAN") {
        if (tokens.size() != 3) {
            return "ERROR usage: SCAN <start> <end>\r\n";
        }

        const auto pairs = store_.Scan(tokens[1], tokens[2]);
        std::ostringstream output;
        output << "RESULT " << pairs.size();
        for (const auto& pair : pairs) {
            output << ' ' << pair.first << '=' << pair.second;
        }
        output << "\r\n";
        return output.str();
    }

    if (command == "QUIT") {
        return "BYE\r\n";
    }

    if (command == "PUT") {
        const std::string::size_type command_end = trimmed.find(' ');
        if (command_end == std::string::npos) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        const std::string remainder = Trim(trimmed.substr(command_end + 1));
        const std::string::size_type key_end = remainder.find(' ');
        if (key_end == std::string::npos) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        const std::string key = remainder.substr(0, key_end);
        const std::string value = remainder.substr(key_end + 1);
        if (key.empty() || value.empty()) {
            return "ERROR usage: PUT <key> <value>\r\n";
        }

        return store_.Put(key, value) ? "OK PUT\r\n" : "OK UPDATE\r\n";
    }

    return "ERROR unknown command\r\n";
}

std::vector<std::string> CommandProcessor::Tokenize(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
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
