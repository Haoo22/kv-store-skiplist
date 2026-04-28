#pragma once

#include "kvstore/kvstore.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace kvstore {

// 服务端网络参数。
struct ServerOptions {
    std::string host {"0.0.0.0"};
    std::uint16_t port {6380};
    int backlog {128};
    int max_events {64};
};

// 基于单线程 Reactor 模型的 TCP 服务端。
class ReactorServer {
public:
    ReactorServer(EngineOptions engine_options, ServerOptions server_options);
    ~ReactorServer();

    ReactorServer(const ReactorServer&) = delete;
    ReactorServer& operator=(const ReactorServer&) = delete;
    ReactorServer(ReactorServer&&) noexcept = delete;
    ReactorServer& operator=(ReactorServer&&) noexcept = delete;

    void Run();

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace kvstore
