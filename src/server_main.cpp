#include "kvstore/Server.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

namespace {

void PrintUsage() {
    std::cerr << "Usage: ./bin/kvstore_server [--no-wal] [--wal-sync-ms <ms>]\n"
                 "  --no-wal         disable WAL and run the single-thread Reactor server in memory-only mode\n"
                 "  --wal-sync-ms    WAL fsync interval in milliseconds (default 10, 0 means sync every write)\n";
}

int ParseNonNegativeInt(const char* text, const char* field_name) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(std::string("invalid ") + field_name);
    }
    return static_cast<int>(value);
}

}  // namespace

int main(int argc, char** argv) {
    kvstore::EngineOptions engine_options;
    kvstore::ServerOptions server_options;
    engine_options.wal_sync_interval_ms = 10;

    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--help") == 0 || std::strcmp(argv[index], "-h") == 0) {
            PrintUsage();
            return 0;
        }

        if (std::strcmp(argv[index], "--no-wal") == 0) {
            engine_options.enable_wal = false;
            continue;
        }

        if (std::strcmp(argv[index], "--wal-sync-ms") == 0) {
            if (index + 1 >= argc) {
                PrintUsage();
                return 1;
            }
            try {
                engine_options.wal_sync_interval_ms =
                    ParseNonNegativeInt(argv[++index], "wal sync interval");
            } catch (const std::exception&) {
                PrintUsage();
                std::cerr << "Server startup failed: invalid command line arguments\n";
                return 1;
            }
            if (engine_options.wal_sync_interval_ms < 0) {
                std::cerr << "WAL sync interval must be non-negative\n";
                return 1;
            }
            continue;
        }

        PrintUsage();
        return 1;
    }

    try {
        kvstore::ReactorServer server(engine_options, server_options);
        std::cout << "KVStore single-thread Reactor server listening on "
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
