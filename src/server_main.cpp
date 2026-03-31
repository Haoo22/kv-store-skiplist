#include "kvstore/Server.hpp"

#include <exception>
#include <iostream>

int main() {
    kvstore::EngineOptions engine_options;
    kvstore::ServerOptions server_options;

    try {
        kvstore::ReactorServer server(engine_options, server_options);
        std::cout << "KVStore server listening on "
                  << server_options.host << ':' << server_options.port
                  << ", WAL path: " << engine_options.wal_path << '\n';
        server.Run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Server startup failed: " << ex.what() << '\n';
        return 1;
    }
}
