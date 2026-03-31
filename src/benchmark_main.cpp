#include <chrono>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
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

std::string ReadLine(int fd) {
    std::string buffer;
    char ch = '\0';

    while (true) {
        const ssize_t bytes = ::read(fd, &ch, 1);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw MakeErrno("read failed");
        }
        if (bytes == 0) {
            throw std::runtime_error("server closed connection");
        }

        buffer.push_back(ch);
        const std::size_t size = buffer.size();
        if (size >= 2 && buffer[size - 2] == '\r' && buffer[size - 1] == '\n') {
            return buffer;
        }
    }
}

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const std::uint16_t port = argc > 2 ? static_cast<std::uint16_t>(std::stoi(argv[2])) : 6380U;
    const int operations = argc > 3 ? std::stoi(argv[3]) : 1000;

    if (operations <= 0) {
        std::cerr << "Benchmark error: operations must be positive\n";
        return 1;
    }

    try {
        SocketHandle socket = Connect(host, port);

        auto run_phase = [&](const std::string& name, const std::string& verb) {
            const auto begin = std::chrono::steady_clock::now();
            for (int index = 0; index < operations; ++index) {
                const std::string key = "bench-key-" + std::to_string(index);
                const std::string value = "bench-value-" + std::to_string(index);
                std::string command;

                if (verb == "PUT") {
                    command = "PUT " + key + ' ' + value + "\r\n";
                } else {
                    command = "GET " + key + "\r\n";
                }

                WriteAll(socket.get(), command);
                const std::string response = ReadLine(socket.get());
                if (verb == "PUT" && !StartsWith(response, "OK ")) {
                    throw std::runtime_error("unexpected PUT response: " + response);
                }
                if (verb == "GET" && !StartsWith(response, "VALUE ")) {
                    throw std::runtime_error("unexpected GET response: " + response);
                }
            }

            const auto end = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                end - begin);
            const double seconds = static_cast<double>(elapsed.count()) / 1000000.0;
            const double throughput = static_cast<double>(operations) / seconds;
            const double latency_us = static_cast<double>(elapsed.count()) /
                                      static_cast<double>(operations);

            std::cout << std::left << std::setw(8) << name
                      << " ops=" << operations
                      << " elapsed=" << std::fixed << std::setprecision(4) << seconds << "s"
                      << " throughput=" << std::setprecision(2) << throughput << " ops/s"
                      << " avg_latency=" << std::setprecision(2) << latency_us << " us\n";
        };

        run_phase("PUT", "PUT");
        run_phase("GET", "GET");

        WriteAll(socket.get(), "QUIT\r\n");
        static_cast<void>(ReadLine(socket.get()));
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Benchmark error: " << ex.what() << '\n';
        return 1;
    }
}
