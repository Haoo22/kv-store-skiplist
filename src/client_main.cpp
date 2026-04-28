#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class SocketHandle {
public:
    explicit SocketHandle(int fd = -1) noexcept : fd_(fd) {}
    ~SocketHandle() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept {
        return fd_;
    }

private:
    int fd_;
};

std::runtime_error MakeErrno(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

SocketHandle Connect(const std::string& host, std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw MakeErrno("socket failed");
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("invalid server address: " + host);
    }

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(fd);
        throw MakeErrno("connect failed");
    }

    int option = 1;
    if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option)) < 0) {
        ::close(fd);
        throw MakeErrno("setsockopt(TCP_NODELAY) failed");
    }

    return SocketHandle(fd);
}

void WriteAll(int fd, const std::string& payload) {
    std::size_t total = 0;
    while (total < payload.size()) {
        const ssize_t written = ::write(fd,
                                        payload.data() + total,
                                        payload.size() - total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw MakeErrno("write failed");
        }
        total += static_cast<std::size_t>(written);
    }
}

class LineReader {
public:
    std::string ReadLine(int fd) {
        while (true) {
            const std::string::size_type pos = buffer_.find("\r\n");
            if (pos != std::string::npos) {
                std::string line = buffer_.substr(0, pos + 2);
                buffer_.erase(0, pos + 2);
                return line;
            }

            char chunk[4096];
            const ssize_t bytes = ::read(fd, chunk, sizeof(chunk));
            if (bytes < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw MakeErrno("read failed");
            }
            if (bytes == 0) {
                throw std::runtime_error("server closed connection");
            }

            // 服务端响应同样按 CRLF 分隔，这里复用简单缓冲读取整行。
            buffer_.append(chunk, static_cast<std::size_t>(bytes));
        }
    }

private:
    std::string buffer_;
};

std::string Trim(const std::string& text) {
    const std::string::size_type begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const std::string::size_type end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> ParseInteractiveCommand(const std::string& line) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return {};
    }

    std::istringstream stream(trimmed);
    std::string command;
    stream >> command;

    if (command == "PUT" || command == "put" || command == "Put") {
        std::string key;
        stream >> key;
        std::string value;
        std::getline(stream, value);
        const std::string trimmed_value = Trim(value);
        if (key.empty() || trimmed_value.empty()) {
            throw std::invalid_argument("PUT requires key and value");
        }
        return {command, key, trimmed_value};
    }

    std::vector<std::string> tokens;
    tokens.push_back(command);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string EncodeResp(const std::vector<std::string>& tokens) {
    std::string output = "*" + std::to_string(tokens.size()) + "\r\n";
    for (const std::string& token : tokens) {
        output.append("$");
        output.append(std::to_string(token.size()));
        output.append("\r\n");
        output.append(token);
        output.append("\r\n");
    }
    return output;
}

void PrintUsage() {
    std::cerr << "Usage: ./bin/kvstore_client [--raw-resp] [host] [port]\n"
                 "  --raw-resp: read RESP-like frames from stdin and forward them as-is\n"
                 "  host: server IPv4 address (default 127.0.0.1)\n"
                 "  port: server port (default 6380)\n";
}

std::uint16_t ParsePort(const char* text) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 ||
        value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("invalid port");
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            PrintUsage();
            return 0;
        }
    }

    std::string host = "127.0.0.1";
    std::uint16_t port = 6380U;
    bool raw_resp = false;

    try {
        int arg_index = 1;
        if (argc > arg_index && std::string(argv[arg_index]) == "--raw-resp") {
            raw_resp = true;
            ++arg_index;
        }
        host = argc > arg_index ? argv[arg_index] : "127.0.0.1";
        port = argc > arg_index + 1 ? ParsePort(argv[arg_index + 1]) : 6380U;
    } catch (const std::exception&) {
        PrintUsage();
        std::cerr << "Client error: invalid command line arguments\n";
        return 1;
    }

    try {
        SocketHandle socket = Connect(host, port);
        LineReader reader;
        std::cout << "Connected to " << host << ':' << port << '\n';
        if (raw_resp) {
            std::cout << "Streaming RESP frames from stdin\n";
        } else {
            std::cout << "Type commands like PING, PUT key value, GET key, SCAN a z, DEL key, CHECKPOINT, QUIT\n";
        }

        std::string line;
        std::string raw_buffer;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            std::vector<std::string> tokens;
            if (raw_resp) {
                raw_buffer.append(line);
                raw_buffer.append("\n");
                if (line == ".") {
                    raw_buffer.erase(raw_buffer.size() - 2);
                    WriteAll(socket.get(), raw_buffer);
                    raw_buffer.clear();
                } else {
                    continue;
                }
            } else {
                tokens = ParseInteractiveCommand(line);
                if (tokens.empty()) {
                    continue;
                }

                // 客户端对交互命令做本地分词，再编码成 RESP-like 请求。
                WriteAll(socket.get(), EncodeResp(tokens));
            }
            const std::string response = reader.ReadLine(socket.get());
            std::cout << response;
            if (!raw_resp &&
                (tokens.front() == "QUIT" || tokens.front() == "quit" || tokens.front() == "Quit")) {
                break;
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << '\n';
        return 1;
    }
}
