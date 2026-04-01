#include "kvstore/Server.hpp"

#include <cstring>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    kvstore::EngineOptions engine_options;
    kvstore::ServerOptions server_options;
    engine_options.wal_sync_interval_ms = 10;

    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--no-wal") == 0) {
            engine_options.enable_wal = false;
            continue;
        }

        if (std::strcmp(argv[index], "--wal-sync-ms") == 0) {
            if (index + 1 >= argc) {
                std::cerr << "Usage: ./bin/kvstore_server [--no-wal] [--wal-sync-ms <ms>]\n";
                return 1;
            }
            engine_options.wal_sync_interval_ms = std::stoi(argv[++index]);
            if (engine_options.wal_sync_interval_ms < 0) {
                std::cerr << "WAL sync interval must be non-negative\n";
                return 1;
            }
            continue;
        }

        std::cerr << "Usage: ./bin/kvstore_server [--no-wal] [--wal-sync-ms <ms>]\n";
        return 1;
    }

    try {
        kvstore::ReactorServer server(engine_options, server_options);
        std::cout << "KVStore server listening on "
                  << server_options.host << ':' << server_options.port
                  << ", WAL "
                  << (engine_options.enable_wal ? "enabled" : "disabled");
        if (engine_options.enable_wal) {
            std::cout << ", path: " << engine_options.wal_path
                      << ", sync_ms: " << engine_options.wal_sync_interval_ms;
        }
        std::cout << '\n';
        server.Run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Server startup failed: " << ex.what() << '\n';
        return 1;
    }
}
