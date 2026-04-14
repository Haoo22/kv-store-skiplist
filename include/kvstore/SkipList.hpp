#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

namespace kvstore {

template <typename Key, typename Value, typename Compare = std::less<Key>>
class SkipList {
public:
    using value_type = std::pair<Key, Value>;

    explicit SkipList(std::size_t max_level = 16,
                      double probability = 0.5,
                      Compare compare = Compare())
        : max_level_(max_level == 0 ? 1 : max_level),
          probability_(probability <= 0.0 || probability >= 1.0 ? 0.5 : probability),
          compare_(std::move(compare)),
          head_(std::make_unique<Node>(max_level_ - 1, Key{}, Value{})),
          current_level_(1),
          size_(0) {}

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;
    SkipList(SkipList&&) = delete;
    SkipList& operator=(SkipList&&) = delete;

    ~SkipList() {
        Clear();
    }

    bool Put(const Key& key, const Value& value) {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        std::unique_lock<std::shared_timed_mutex> range_lock(range_mutex_);

        while (true) {
            std::vector<Node*> predecessors(max_level_, nullptr);
            std::vector<Node*> successors(max_level_, nullptr);
            const int found_level = FindNode(key, predecessors, successors);
            if (found_level >= 0) {
                Node* existing = successors[static_cast<std::size_t>(found_level)];
                std::unique_lock<std::mutex> node_lock(existing->mutex);
                if (existing->marked.load(std::memory_order_acquire)) {
                    continue;
                }
                if (!existing->fully_linked.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                    continue;
                }
                existing->value = value;
                return false;
            }

            const std::size_t node_level = RandomLevel() - 1;
            std::vector<Node*> lock_targets;
            lock_targets.reserve(node_level + 1);
            for (std::size_t level = 0; level <= node_level; ++level) {
                lock_targets.push_back(predecessors[level]);
            }

            const auto locks = LockNodes(lock_targets);
            if (!CanInsert(predecessors, successors, node_level)) {
                continue;
            }

            auto new_node = std::make_unique<Node>(node_level, key, value);
            Node* new_node_ptr = new_node.get();
            for (std::size_t level = 0; level <= node_level; ++level) {
                new_node_ptr->forward[level].store(successors[level], std::memory_order_relaxed);
            }

            {
                std::lock_guard<std::mutex> ownership_lock(ownership_mutex_);
                owned_nodes_.push_back(std::move(new_node));
            }

            for (std::size_t level = 0; level <= node_level; ++level) {
                predecessors[level]->forward[level].store(new_node_ptr, std::memory_order_release);
            }

            new_node_ptr->fully_linked.store(true, std::memory_order_release);
            size_.fetch_add(1, std::memory_order_relaxed);
            RaiseCurrentLevel(node_level + 1);
            return true;
        }
    }

    bool Get(const Key& key, Value* value) const {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);

        Node* candidate = FindNodeNoLock(key);
        if (candidate == nullptr) {
            return false;
        }
        if (candidate->marked.load(std::memory_order_acquire) ||
            !candidate->fully_linked.load(std::memory_order_acquire)) {
            return false;
        }

        std::lock_guard<std::mutex> node_lock(candidate->mutex);
        if (candidate->marked.load(std::memory_order_acquire) ||
            !candidate->fully_linked.load(std::memory_order_acquire)) {
            return false;
        }

        if (value != nullptr) {
            *value = candidate->value;
        }
        return true;
    }

    bool Delete(const Key& key) {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        std::unique_lock<std::shared_timed_mutex> range_lock(range_mutex_);

        while (true) {
            std::vector<Node*> predecessors(max_level_, nullptr);
            std::vector<Node*> successors(max_level_, nullptr);
            const int found_level = FindNode(key, predecessors, successors);
            if (found_level < 0) {
                return false;
            }

            Node* victim = successors[static_cast<std::size_t>(found_level)];
            if (victim == nullptr) {
                return false;
            }

            const std::size_t victim_level = victim->top_level;
            std::vector<Node*> lock_targets;
            lock_targets.reserve(victim_level + 2);
            for (std::size_t level = 0; level <= victim_level; ++level) {
                lock_targets.push_back(predecessors[level]);
            }
            lock_targets.push_back(victim);

            const auto locks = LockNodes(lock_targets);
            if (!victim->fully_linked.load(std::memory_order_acquire) ||
                victim->marked.load(std::memory_order_acquire) ||
                !KeysEqual(victim->key, key) ||
                !CanDelete(predecessors, victim)) {
                continue;
            }

            victim->marked.store(true, std::memory_order_release);
            for (std::size_t level = victim_level + 1; level > 0; --level) {
                predecessors[level - 1]->forward[level - 1].store(
                    victim->forward[level - 1].load(std::memory_order_acquire),
                    std::memory_order_release);
            }

            size_.fetch_sub(1, std::memory_order_relaxed);
            RecomputeCurrentLevel();
            return true;
        }
    }

    std::vector<value_type> Scan(const Key& start, const Key& end) const {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        std::shared_lock<std::shared_timed_mutex> range_lock(range_mutex_);
        std::vector<value_type> result;

        if (compare_(end, start)) {
            return result;
        }

        Node* current = head_.get();
        const std::size_t active_levels = current_level_.load(std::memory_order_acquire);
        for (std::size_t level = active_levels; level > 0; --level) {
            Node* next = current->forward[level - 1].load(std::memory_order_acquire);
            while (next != nullptr && compare_(next->key, start)) {
                current = next;
                next = current->forward[level - 1].load(std::memory_order_acquire);
            }
        }

        current = current->forward[0].load(std::memory_order_acquire);
        while (current != nullptr && !compare_(end, current->key)) {
            if (!current->marked.load(std::memory_order_acquire) &&
                current->fully_linked.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> node_lock(current->mutex);
                if (!current->marked.load(std::memory_order_acquire) &&
                    current->fully_linked.load(std::memory_order_acquire)) {
                    result.emplace_back(current->key, current->value);
                }
            }
            current = current->forward[0].load(std::memory_order_acquire);
        }

        return result;
    }

    std::size_t Size() const noexcept {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        return size_.load(std::memory_order_acquire);
    }

    bool Empty() const noexcept {
        return Size() == 0;
    }

    void Clear() {
        std::unique_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        std::unique_lock<std::shared_timed_mutex> range_lock(range_mutex_);
        for (std::size_t level = 0; level < max_level_; ++level) {
            head_->forward[level].store(nullptr, std::memory_order_release);
        }

        {
            std::lock_guard<std::mutex> ownership_lock(ownership_mutex_);
            owned_nodes_.clear();
        }

        current_level_.store(1, std::memory_order_release);
        size_.store(0, std::memory_order_release);
    }

    std::size_t CurrentLevel() const noexcept {
        std::shared_lock<std::shared_timed_mutex> lifecycle_lock(lifecycle_mutex_);
        return current_level_.load(std::memory_order_acquire);
    }

private:
    struct Node {
        Node(std::size_t node_top_level, Key node_key, Value node_value)
            : key(std::move(node_key)),
              value(std::move(node_value)),
              top_level(node_top_level),
              forward(node_top_level + 1),
              marked(false),
              fully_linked(false) {
            for (std::atomic<Node*>& pointer : forward) {
                pointer.store(nullptr, std::memory_order_relaxed);
            }
        }

        Key key;
        Value value;
        const std::size_t top_level;
        mutable std::mutex mutex;
        std::vector<std::atomic<Node*>> forward;
        std::atomic<bool> marked;
        std::atomic<bool> fully_linked;
    };

    static std::vector<std::unique_lock<std::mutex>> LockNodes(std::vector<Node*> nodes) {
        std::sort(nodes.begin(), nodes.end());
        nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());

        std::vector<std::unique_lock<std::mutex>> locks;
        locks.reserve(nodes.size());
        for (Node* node : nodes) {
            if (node != nullptr) {
                locks.emplace_back(node->mutex);
            }
        }
        return locks;
    }

    bool KeysEqual(const Key& lhs, const Key& rhs) const {
        return !compare_(lhs, rhs) && !compare_(rhs, lhs);
    }

    int FindNode(const Key& key,
                 std::vector<Node*>& predecessors,
                 std::vector<Node*>& successors) const {
        Node* predecessor = head_.get();
        int found_level = -1;
        const std::size_t active_levels = current_level_.load(std::memory_order_acquire);

        for (std::size_t level = active_levels; level > 0; --level) {
            Node* current = predecessor->forward[level - 1].load(std::memory_order_acquire);
            while (current != nullptr && compare_(current->key, key)) {
                predecessor = current;
                current = predecessor->forward[level - 1].load(std::memory_order_acquire);
            }

            if (found_level == -1 && current != nullptr && KeysEqual(current->key, key)) {
                found_level = static_cast<int>(level - 1);
            }
            predecessors[level - 1] = predecessor;
            successors[level - 1] = current;
        }

        for (std::size_t level = active_levels; level < max_level_; ++level) {
            predecessors[level] = head_.get();
            successors[level] = nullptr;
        }

        return found_level;
    }

    Node* FindNodeNoLock(const Key& key) const {
        Node* predecessor = head_.get();
        const std::size_t active_levels = current_level_.load(std::memory_order_acquire);

        for (std::size_t level = active_levels; level > 0; --level) {
            Node* current = predecessor->forward[level - 1].load(std::memory_order_acquire);
            while (current != nullptr && compare_(current->key, key)) {
                predecessor = current;
                current = predecessor->forward[level - 1].load(std::memory_order_acquire);
            }
        }

        Node* candidate = predecessor->forward[0].load(std::memory_order_acquire);
        if (candidate != nullptr && KeysEqual(candidate->key, key)) {
            return candidate;
        }
        return nullptr;
    }

    bool CanInsert(const std::vector<Node*>& predecessors,
                   const std::vector<Node*>& successors,
                   std::size_t node_level) const {
        for (std::size_t level = 0; level <= node_level; ++level) {
            Node* predecessor = predecessors[level];
            Node* successor = successors[level];
            if (predecessor == nullptr) {
                return false;
            }
            if (predecessor->marked.load(std::memory_order_acquire)) {
                return false;
            }
            if (successor != nullptr && successor->marked.load(std::memory_order_acquire)) {
                return false;
            }
            if (predecessor->forward[level].load(std::memory_order_acquire) != successor) {
                return false;
            }
        }
        return true;
    }

    bool CanDelete(const std::vector<Node*>& predecessors, Node* victim) const {
        for (std::size_t level = 0; level <= victim->top_level; ++level) {
            Node* predecessor = predecessors[level];
            if (predecessor == nullptr ||
                predecessor->marked.load(std::memory_order_acquire) ||
                predecessor->forward[level].load(std::memory_order_acquire) != victim) {
                return false;
            }
        }
        return true;
    }

    std::size_t RandomLevel() const {
        thread_local std::mt19937 engine(SeedForThread());
        std::bernoulli_distribution distribution(probability_);
        std::size_t level = 1;
        while (level < max_level_ && distribution(engine)) {
            ++level;
        }
        return level;
    }

    static unsigned int SeedForThread() {
        std::random_device device;
        const unsigned int random_seed = device();
        const auto thread_hash = std::hash<std::thread::id> {}(std::this_thread::get_id());
        return random_seed ^ static_cast<unsigned int>(thread_hash);
    }

    void RaiseCurrentLevel(std::size_t candidate) {
        std::size_t observed = current_level_.load(std::memory_order_acquire);
        while (candidate > observed &&
               !current_level_.compare_exchange_weak(observed,
                                                     candidate,
                                                     std::memory_order_release,
                                                     std::memory_order_acquire)) {
        }
    }

    void RecomputeCurrentLevel() {
        std::size_t level = max_level_;
        while (level > 1 &&
               head_->forward[level - 1].load(std::memory_order_acquire) == nullptr) {
            --level;
        }
        current_level_.store(level, std::memory_order_release);
    }

    const std::size_t max_level_;
    const double probability_;
    Compare compare_;

    mutable std::shared_timed_mutex lifecycle_mutex_;
    mutable std::shared_timed_mutex range_mutex_;
    mutable std::mutex ownership_mutex_;
    std::unique_ptr<Node> head_;
    std::vector<std::unique_ptr<Node>> owned_nodes_;
    std::atomic<std::size_t> current_level_;
    std::atomic<std::size_t> size_;
};

}  // namespace kvstore
