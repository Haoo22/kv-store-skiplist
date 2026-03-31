#include "kvstore/kvstore.hpp"

namespace kvstore {

class KVStore::Impl {
public:
    Impl() = default;
    ~Impl() = default;
};

KVStore::KVStore(EngineOptions options)
    : options_(std::move(options)), impl_(std::make_unique<Impl>()) {}

KVStore::~KVStore() = default;

const EngineOptions& KVStore::options() const noexcept {
    return options_;
}

}  // namespace kvstore
