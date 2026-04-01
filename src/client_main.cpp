#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

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

            buffer_.append(chunk, static_cast<std::size_t>(bytes));
        }
    }

private:
    std::string buffer_;
};

}  // namespace

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const std::uint16_t port = argc > 2 ? static_cast<std::uint16_t>(std::stoi(argv[2])) : 6380U;

    try {
        SocketHandle socket = Connect(host, port);
        LineReader reader;
        std::cout << "Connected to " << host << ':' << port << '\n';
        std::cout << "Type commands like PUT key value, GET key, SCAN a z, DEL key, QUIT\n";

        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            WriteAll(socket.get(), line + "\r\n");
            const std::string response = reader.ReadLine(socket.get());
            std::cout << response;
            if (line == "QUIT" || line == "quit" || line == "Quit") {
                break;
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << '\n';
        return 1;
    }
}
