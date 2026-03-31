#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
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
          current_level_(1),
          size_(0),
          compare_(std::move(compare)),
          head_(std::make_shared<Node>(max_level_, Key{}, Value{})),
          random_engine_(std::random_device{}()),
          level_distribution_(probability_) {}

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;
    SkipList(SkipList&&) = delete;
    SkipList& operator=(SkipList&&) = delete;

    bool Put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Node>> update(max_level_, head_);
        auto current = FindPredecessors(key, update);

        if (current != nullptr && KeysEqual(current->key, key)) {
            current->value = value;
            return false;
        }

        const std::size_t node_level = RandomLevel();
        if (node_level > current_level_) {
            for (std::size_t level = current_level_; level < node_level; ++level) {
                update[level] = head_;
            }
            current_level_ = node_level;
        }

        auto new_node = std::make_shared<Node>(node_level, key, value);
        for (std::size_t level = 0; level < node_level; ++level) {
            new_node->forward[level] = update[level]->forward[level];
            update[level]->forward[level] = new_node;
        }

        ++size_;
        return true;
    }

    bool Get(const Key& key, Value* value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto current = FindNode(key);
        if (current == nullptr) {
            return false;
        }

        if (value != nullptr) {
            *value = current->value;
        }
        return true;
    }

    bool Delete(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Node>> update(max_level_, head_);
        auto current = FindPredecessors(key, update);

        if (current == nullptr || !KeysEqual(current->key, key)) {
            return false;
        }

        for (std::size_t level = 0; level < current_level_; ++level) {
            if (update[level]->forward[level] != current) {
                continue;
            }
            update[level]->forward[level] = current->forward[level];
        }

        while (current_level_ > 1 && head_->forward[current_level_ - 1] == nullptr) {
            --current_level_;
        }

        --size_;
        return true;
    }

    std::vector<value_type> Scan(const Key& start, const Key& end) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<value_type> result;

        if (compare_(end, start)) {
            return result;
        }

        auto current = head_;
        for (std::size_t level = current_level_; level > 0; --level) {
            while (current->forward[level - 1] != nullptr &&
                   compare_(current->forward[level - 1]->key, start)) {
                current = current->forward[level - 1];
            }
        }

        current = current->forward[0];
        while (current != nullptr && !compare_(end, current->key)) {
            result.emplace_back(current->key, current->value);
            current = current->forward[0];
        }

        return result;
    }

    std::size_t Size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    bool Empty() const noexcept {
        return Size() == 0;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t level = 0; level < max_level_; ++level) {
            head_->forward[level].reset();
        }
        current_level_ = 1;
        size_ = 0;
    }

    std::size_t CurrentLevel() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_level_;
    }

private:
    struct Node {
        Node(std::size_t level, Key node_key, Value node_value)
            : key(std::move(node_key)),
              value(std::move(node_value)),
              forward(level, nullptr) {}

        Key key;
        Value value;
        std::vector<std::shared_ptr<Node>> forward;
    };

    bool KeysEqual(const Key& lhs, const Key& rhs) const {
        return !compare_(lhs, rhs) && !compare_(rhs, lhs);
    }

    std::shared_ptr<Node> FindPredecessors(
        const Key& key,
        std::vector<std::shared_ptr<Node>>& update) const {
        auto current = head_;
        for (std::size_t level = current_level_; level > 0; --level) {
            while (current->forward[level - 1] != nullptr &&
                   compare_(current->forward[level - 1]->key, key)) {
                current = current->forward[level - 1];
            }
            update[level - 1] = current;
        }
        return current->forward[0];
    }

    std::shared_ptr<Node> FindNode(const Key& key) const {
        std::vector<std::shared_ptr<Node>> update(max_level_, head_);
        auto current = FindPredecessors(key, update);
        if (current != nullptr && KeysEqual(current->key, key)) {
            return current;
        }
        return nullptr;
    }

    std::size_t RandomLevel() {
        std::size_t level = 1;
        while (level < max_level_ && level_distribution_(random_engine_)) {
            ++level;
        }
        return level;
    }

    const std::size_t max_level_;
    const double probability_;

    mutable std::mutex mutex_;
    std::size_t current_level_;
    std::size_t size_;
    Compare compare_;
    std::shared_ptr<Node> head_;
    mutable std::mt19937 random_engine_;
    mutable std::bernoulli_distribution level_distribution_;
};

}  // namespace kvstore
