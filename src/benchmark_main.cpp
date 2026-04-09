#include <chrono>
#include <cerrno>
#include <cstring>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
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

            buffer_.append(chunk, static_cast<std::size_t>(bytes));
        }
    }

private:
    std::string buffer_;
};

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

enum class Scenario {
    kPutGet,
    kFullProtocol,
};

struct PhaseDefinition {
    std::string name;
    int requests_per_operation {0};
};

struct RunOptions {
    std::string host;
    std::uint16_t port {6380U};
    int operations {1000};
    int pipeline_depth {1};
    Scenario scenario {Scenario::kPutGet};
    int clients {1};
};

Scenario ParseScenario(const std::string& text) {
    if (text == "put-get") {
        return Scenario::kPutGet;
    }
    if (text == "full") {
        return Scenario::kFullProtocol;
    }
    throw std::invalid_argument("invalid scenario: " + text);
}

std::vector<PhaseDefinition> BuildPhaseDefinitions(Scenario scenario) {
    std::vector<PhaseDefinition> phases;
    if (scenario == Scenario::kFullProtocol) {
        phases.push_back({"PING", 1});
    }
    phases.push_back({"PUT", 1});
    phases.push_back({"GET", 1});
    if (scenario == Scenario::kFullProtocol) {
        phases.push_back({"SCAN", 1});
        phases.push_back({"DEL", 1});
    }
    return phases;
}

void PrintUsage() {
    std::cerr << "Usage: ./bin/kvstore_bench [host] [port] [operations] [pipeline_depth]"
                 " [scenario] [clients]\n"
                 "  host: IPv4 server address (default 127.0.0.1)\n"
                 "  port: server port (default 6380)\n"
                 "  operations: requests per phase per client (default 1000)\n"
                 "  pipeline_depth: in-flight requests per batch (default 1)\n"
                 "  scenario: put-get (default) | full\n"
                 "  clients: concurrent benchmark clients (default 1)\n";
}

int ParsePositiveInt(const char* text, const char* field_name) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(std::string("invalid ") + field_name);
    }
    return static_cast<int>(value);
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

    RunOptions options;
    try {
        options.host = argc > 1 ? argv[1] : "127.0.0.1";
        options.port = argc > 2 ? ParsePort(argv[2]) : 6380U;
        options.operations = argc > 3 ? ParsePositiveInt(argv[3], "operations") : 1000;
        options.pipeline_depth =
            argc > 4 ? ParsePositiveInt(argv[4], "pipeline depth") : 1;
        options.scenario = argc > 5 ? ParseScenario(argv[5]) : Scenario::kPutGet;
        options.clients = argc > 6 ? ParsePositiveInt(argv[6], "clients") : 1;
    } catch (const std::invalid_argument& ex) {
        PrintUsage();
        std::cerr << "Benchmark error: " << ex.what() << '\n';
        return 1;
    }

    try {
        const std::vector<PhaseDefinition> phases = BuildPhaseDefinitions(options.scenario);

        auto run_client = [&](int client_index, bool print_phase_stats) {
            SocketHandle socket = Connect(options.host, options.port);
            LineReader reader;

            auto run_phase =
                [&](const std::string& name,
                    const std::function<std::string(int)>& build_request,
                    const std::function<void(const std::string&)>& validate_response) {
                const auto begin = std::chrono::steady_clock::now();
                for (int index = 0; index < options.operations; index += options.pipeline_depth) {
                    const int batch_size =
                        std::min(options.pipeline_depth, options.operations - index);
                    std::string batch;

                    for (int offset = 0; offset < batch_size; ++offset) {
                        const int current = index + offset;
                        batch.append(build_request(current));
                    }

                    WriteAll(socket.get(), batch);
                    for (int offset = 0; offset < batch_size; ++offset) {
                        const std::string response = reader.ReadLine(socket.get());
                        validate_response(response);
                    }
                }

                const auto end = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    end - begin);
                const double seconds = static_cast<double>(elapsed.count()) / 1000000.0;
                const double throughput = static_cast<double>(options.operations) / seconds;
                const double latency_us = static_cast<double>(elapsed.count()) /
                                          static_cast<double>(options.operations);

                if (print_phase_stats) {
                    std::cout << std::left << std::setw(8) << name
                              << " ops=" << options.operations
                              << " pipeline=" << options.pipeline_depth
                              << " elapsed=" << std::fixed << std::setprecision(4) << seconds
                              << "s"
                              << " throughput=" << std::setprecision(2) << throughput
                              << " ops/s"
                              << " avg_latency=" << std::setprecision(2) << latency_us
                              << " us\n";
                }
            };

            const std::string key_prefix = "bench-c" + std::to_string(client_index) + "-key-";
            const std::string value_prefix = "bench-c" + std::to_string(client_index) + "-value-";
            auto make_key = [&](int index) {
                return key_prefix + std::to_string(index);
            };
            auto make_value = [&](int index) {
                return value_prefix + std::to_string(index);
            };

            auto expect_prefix = [](const std::string& phase,
                                    const std::string& prefix,
                                    const std::string& response) {
                if (!StartsWith(response, prefix)) {
                    throw std::runtime_error("unexpected " + phase + " response: " + response);
                }
            };

            if (options.scenario == Scenario::kFullProtocol) {
                run_phase("PING",
                          [](int) {
                              return std::string("PING\r\n");
                          },
                          [&](const std::string& response) {
                              expect_prefix("PING", "PONG\r\n", response);
                          });
            }

            run_phase("PUT",
                      [&](int index) {
                          return "PUT " + make_key(index) + " " + make_value(index) + "\r\n";
                      },
                      [&](const std::string& response) {
                          if (!StartsWith(response, "OK PUT\r\n") &&
                              !StartsWith(response, "OK UPDATE\r\n")) {
                              throw std::runtime_error("unexpected PUT response: " + response);
                          }
                      });

            run_phase("GET",
                      [&](int index) {
                          return "GET " + make_key(index) + "\r\n";
                      },
                      [&](const std::string& response) {
                          expect_prefix("GET", "VALUE ", response);
                      });

            if (options.scenario == Scenario::kFullProtocol) {
                run_phase("SCAN",
                          [&](int) {
                              return "SCAN " + key_prefix + "0 " + key_prefix + "zzzzzzzz\r\n";
                          },
                          [&](const std::string& response) {
                              expect_prefix("SCAN", "RESULT ", response);
                          });

                run_phase("DEL",
                          [&](int index) {
                              return "DEL " + make_key(index) + "\r\n";
                          },
                          [&](const std::string& response) {
                              expect_prefix("DEL", "OK DELETE\r\n", response);
                          });
            }

            WriteAll(socket.get(), "QUIT\r\n");
            const std::string quit_response = reader.ReadLine(socket.get());
            if (!StartsWith(quit_response, "BYE\r\n")) {
                throw std::runtime_error("unexpected QUIT response: " + quit_response);
            }
        };

        if (options.clients == 1) {
            run_client(0, true);
        } else {
            std::vector<std::thread> threads;
            std::vector<std::exception_ptr> errors(static_cast<std::size_t>(options.clients));
            threads.reserve(static_cast<std::size_t>(options.clients));

            const auto begin = std::chrono::steady_clock::now();
            for (int client_index = 0; client_index < options.clients; ++client_index) {
                threads.emplace_back([&, client_index]() {
                    try {
                        run_client(client_index, false);
                    } catch (...) {
                        errors[static_cast<std::size_t>(client_index)] = std::current_exception();
                    }
                });
            }

            for (std::thread& thread : threads) {
                thread.join();
            }
            for (const std::exception_ptr& error : errors) {
                if (error != nullptr) {
                    std::rethrow_exception(error);
                }
            }

            const auto end = std::chrono::steady_clock::now();
            const auto elapsed_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
            const double seconds = static_cast<double>(elapsed_ns) / 1000000000.0;
            const long long total_requests =
                static_cast<long long>(options.clients) *
                static_cast<long long>(options.operations) *
                static_cast<long long>(phases.size());
            const double aggregate_qps = static_cast<double>(total_requests) / seconds;

            std::cout << "clients=" << options.clients
                      << " scenario="
                      << (options.scenario == Scenario::kPutGet ? "put-get" : "full")
                      << " pipeline=" << options.pipeline_depth
                      << " ops_per_client=" << options.operations
                      << " total_requests=" << total_requests
                      << " wall_seconds=" << std::fixed << std::setprecision(4) << seconds
                      << " aggregate_qps=" << std::setprecision(2) << aggregate_qps
                      << '\n';
        }
        return 0;
    } catch (const std::invalid_argument& ex) {
        PrintUsage();
        std::cerr << "Benchmark error: " << ex.what() << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Benchmark error: " << ex.what() << '\n';
        return 1;
    }
}
