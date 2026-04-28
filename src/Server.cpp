#include "kvstore/Server.hpp"

#include "kvstore/Protocol.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace kvstore {
namespace {

volatile std::sig_atomic_t g_should_stop = 0;

void HandleSignal(int) {
    // 使用信号安全的简单标志，在事件循环中优雅退出。
    g_should_stop = 1;
}

std::runtime_error MakeErrno(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}
    ~FileDescriptor() { Reset(); }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            Reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int Get() const noexcept { return fd_; }

    void Reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

void SetNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        throw MakeErrno("fcntl(F_GETFL) failed");
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw MakeErrno("fcntl(F_SETFL) failed");
    }
}

void SetReuseAddr(int fd) {
    int option = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        throw MakeErrno("setsockopt(SO_REUSEADDR) failed");
    }
}

void SetTcpNoDelay(int fd) {
    int option = 1;
    if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option)) < 0) {
        throw MakeErrno("setsockopt(TCP_NODELAY) failed");
    }
}

void UpdateInterest(int epoll_fd, int fd, std::uint32_t events) {
    epoll_event event {};
    event.events = events;
    event.data.fd = fd;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) {
        throw MakeErrno("epoll_ctl MOD failed");
    }
}

std::string FirstCommandTokenUpper(const std::string& line) {
    std::string token = line;
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return token;
}

}  // namespace

class ReactorServer::Impl {
public:
    Impl(EngineOptions engine_options, ServerOptions server_options)
        : store_(std::move(engine_options)),
          processor_(store_),
          server_options_(std::move(server_options)),
          listen_fd_(CreateListenSocket(server_options_)),
          epoll_fd_(CreateEpoll()),
          events_(static_cast<std::size_t>(server_options_.max_events)) {
        RegisterSignalHandlers();
        AddListenSocket();
    }

    void Run() {
        while (!g_should_stop) {
            // 定时醒来检查退出标志，避免永久阻塞在 epoll_wait 上。
            const int ready = ::epoll_wait(
                epoll_fd_.Get(),
                events_.data(),
                static_cast<int>(events_.size()),
                1000);

            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw MakeErrno("epoll_wait failed");
            }

            for (int index = 0; index < ready; ++index) {
                const epoll_event& event = events_[static_cast<std::size_t>(index)];
                if (event.data.fd == listen_fd_.Get()) {
                    AcceptConnections();
                    continue;
                }
                HandleConnectionEvent(event);
            }
        }
    }

private:
    struct Connection {
        explicit Connection(int socket_fd)
            : fd(socket_fd) {}

        FileDescriptor fd;
        RequestCodec codec;
        // 非阻塞写场景下，未发完的数据暂存在这里，等待 EPOLLOUT 继续发送。
        std::string write_buffer;
        std::uint32_t interest_events {EPOLLIN | EPOLLRDHUP};
        bool close_after_write {false};
    };

    static int CreateListenSocket(const ServerOptions& options) {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw MakeErrno("socket failed");
        }

        try {
            SetReuseAddr(fd);
            SetNonBlocking(fd);

            sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_port = htons(options.port);
            if (::inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) != 1) {
                throw std::runtime_error("invalid listen address: " + options.host);
            }

            if (::bind(fd,
                       reinterpret_cast<const sockaddr*>(&address),
                       sizeof(address)) < 0) {
                throw MakeErrno("bind failed");
            }

            if (::listen(fd, options.backlog) < 0) {
                throw MakeErrno("listen failed");
            }
        } catch (...) {
            ::close(fd);
            throw;
        }

        return fd;
    }

    static int CreateEpoll() {
        const int fd = ::epoll_create1(0);
        if (fd < 0) {
            throw MakeErrno("epoll_create1 failed");
        }
        return fd;
    }

    void RegisterSignalHandlers() {
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
    }

    void AddListenSocket() {
        epoll_event event {};
        event.events = EPOLLIN;
        event.data.fd = listen_fd_.Get();
        if (::epoll_ctl(epoll_fd_.Get(), EPOLL_CTL_ADD, listen_fd_.Get(), &event) < 0) {
            throw MakeErrno("epoll_ctl ADD listen socket failed");
        }
    }

    void AcceptConnections() {
        while (true) {
            sockaddr_in peer {};
            socklen_t peer_length = sizeof(peer);
            const int client_fd = ::accept(
                listen_fd_.Get(),
                reinterpret_cast<sockaddr*>(&peer),
                &peer_length);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                if (errno == EINTR) {
                    continue;
                }
                throw MakeErrno("accept failed");
            }

            try {
                SetNonBlocking(client_fd);
                SetTcpNoDelay(client_fd);
                epoll_event event {};
                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = client_fd;
                if (::epoll_ctl(epoll_fd_.Get(), EPOLL_CTL_ADD, client_fd, &event) < 0) {
                    throw MakeErrno("epoll_ctl ADD client socket failed");
                }
                connections_.emplace(client_fd, Connection(client_fd));
            } catch (...) {
                ::close(client_fd);
                throw;
            }
        }
    }

    void HandleConnectionEvent(const epoll_event& event) {
        auto iterator = connections_.find(event.data.fd);
        if (iterator == connections_.end()) {
            return;
        }

        // 对端关闭、挂起或出错时直接回收连接状态。
        if ((event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) {
            CloseConnection(iterator);
            return;
        }

        if ((event.events & EPOLLIN) != 0U) {
            if (!ReadFromConnection(iterator)) {
                return;
            }
        }

        if ((event.events & EPOLLOUT) != 0U) {
            if (!WriteToConnection(iterator)) {
                return;
            }
        }
    }

    bool ReadFromConnection(std::unordered_map<int, Connection>::iterator iterator) {
        char buffer[4096];

        while (true) {
            const ssize_t bytes = ::read(iterator->second.fd.Get(), buffer, sizeof(buffer));
            if (bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                CloseConnection(iterator);
                return false;
            }

            if (bytes == 0) {
                CloseConnection(iterator);
                return false;
            }

            iterator->second.codec.Append(buffer, static_cast<std::size_t>(bytes));
            std::vector<DecodedRequest> requests;
            try {
                requests = iterator->second.codec.ExtractRequests();
            } catch (const std::exception&) {
                CloseConnection(iterator);
                return false;
            }
            for (const DecodedRequest& request : requests) {
                // 连接级别按“完整一条协议请求”驱动命令处理。
                const std::string response = processor_.Execute(request.tokens);
                const bool is_quit = !request.tokens.empty() &&
                                     FirstCommandTokenUpper(request.tokens.front()) == "QUIT";
                if (is_quit) {
                    // QUIT 仍需把 BYE 回给客户端，然后再关闭连接。
                    iterator->second.close_after_write = true;
                }
                iterator->second.write_buffer.append(response);
            }
        }

        if (!iterator->second.write_buffer.empty()) {
            return WriteToConnection(iterator);
        }
        return true;
    }

    bool WriteToConnection(std::unordered_map<int, Connection>::iterator iterator) {
        while (!iterator->second.write_buffer.empty()) {
            const ssize_t bytes = ::write(iterator->second.fd.Get(),
                                          iterator->second.write_buffer.data(),
                                          iterator->second.write_buffer.size());
            if (bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 内核发送缓冲区已满，等待下一次可写事件继续发送。
                    SetConnectionInterest(iterator, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
                    return true;
                }
                if (errno == EINTR) {
                    continue;
                }
                CloseConnection(iterator);
                return false;
            }

            iterator->second.write_buffer.erase(
                0, static_cast<std::size_t>(bytes));
        }

        if (iterator->second.close_after_write) {
            CloseConnection(iterator);
            return false;
        }

        SetConnectionInterest(iterator, EPOLLIN | EPOLLRDHUP);
        return true;
    }

    void SetConnectionInterest(std::unordered_map<int, Connection>::iterator iterator,
                               std::uint32_t events) {
        if (iterator->second.interest_events == events) {
            return;
        }
        UpdateInterest(epoll_fd_.Get(), iterator->second.fd.Get(), events);
        iterator->second.interest_events = events;
    }

    void CloseConnection(std::unordered_map<int, Connection>::iterator iterator) {
        const int fd = iterator->second.fd.Get();
        ::epoll_ctl(epoll_fd_.Get(), EPOLL_CTL_DEL, fd, nullptr);
        // 擦除 map 项时，Connection 内的 FileDescriptor 会自动关闭 socket。
        connections_.erase(iterator);
    }

    KVStore store_;
    CommandProcessor processor_;
    ServerOptions server_options_;
    FileDescriptor listen_fd_;
    FileDescriptor epoll_fd_;
    std::unordered_map<int, Connection> connections_;
    std::vector<epoll_event> events_;
};

ReactorServer::ReactorServer(EngineOptions engine_options, ServerOptions server_options)
    : impl_(new Impl(std::move(engine_options), std::move(server_options))) {}

ReactorServer::~ReactorServer() = default;

void ReactorServer::Run() {
    impl_->Run();
}

}  // namespace kvstore
