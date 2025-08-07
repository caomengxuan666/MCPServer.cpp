#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Astra::datastructures {


    template<typename Key, typename Value>
    class LRUCache {
    public:
        using iterator = typename std::list<std::pair<Key, Value>>::iterator;
        using const_iterator = typename std::list<std::pair<Key, Value>>::const_iterator;
        using clock_type = std::chrono::steady_clock;
        using time_point = std::chrono::time_point<clock_type>;

        explicit LRUCache(size_t capacity, size_t hot_key_threshold = 100, std::chrono::seconds ttl = std::chrono::seconds::zero())
            : capacity_(capacity), hot_key_threshold_(hot_key_threshold), ttl_(ttl),
              cleanup_thread_(nullptr), cleanup_running_(false) {}

        ~LRUCache() {
            StopCleanupThread();
        }

        std::optional<Value> Get(const Key &key) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = cache_.find(key);
            if (it == cache_.end()) {
                return std::nullopt;
            }

            if (IsExpired(it)) {
                Remove(key);
                return std::nullopt;
            }

            MoveToFront(it);
            UpdateHotKey(it->second, key);
            return std::make_optional(it->second->second);
        }

        std::vector<std::optional<Value>> BatchGet(const std::vector<Key> &keys) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::vector<std::optional<Value>> values;
            values.reserve(keys.size());

            for (const auto &key: keys) {
                auto it = cache_.find(key);
                if (it == cache_.end()) {
                    values.emplace_back(std::nullopt);
                    continue;
                }

                if (IsExpired(it)) {
                    Remove(key);
                    values.emplace_back(std::nullopt);
                    continue;
                }

                MoveToFront(it);
                UpdateHotKey(it->second, key);
                values.emplace_back(it->second->second);
            }

            return values;
        }

        void setCacheCapacity(size_t capacity) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            capacity_ = capacity;
        }

        void Put(const Key &key, const Value &value, std::chrono::seconds ttl = std::chrono::seconds::zero()) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            if (capacity_ == 0) {
                Clear();
                return;
            }

            auto it = cache_.find(key);
            if (it != cache_.end()) {
                usage_.erase(it->second);
                cache_.erase(it);
            }

            EnsureCapacity(1);

            usage_.emplace_front(key, value);
            cache_[key] = usage_.begin();
            UpdateHotKey(usage_.begin(), key);

            SetExpiration(key, ttl);
        }

        void BatchPut(const std::vector<Key> &keys, const std::vector<Value> &values,
                      std::chrono::seconds ttl = std::chrono::seconds::zero()) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            if (keys.size() != values.size()) {
                throw std::invalid_argument("keys and values must have the same size");
            }

            if (capacity_ == 0) {
                Clear();
                return;
            }

            size_t required_capacity = keys.size();
            size_t current_size = cache_.size();
            size_t need_to_evict = 0;

            if (current_size + required_capacity > capacity_) {
                need_to_evict = current_size + required_capacity - capacity_;
            }

            EvictLRUBatch(need_to_evict);

            for (size_t i = 0; i < keys.size(); ++i) {
                const auto &key = keys[i];
                const auto &value = values[i];

                auto it = cache_.find(key);
                if (it != cache_.end()) {
                    usage_.erase(it->second);
                    cache_.erase(it);
                }

                usage_.emplace_front(key, value);
                cache_[key] = usage_.begin();
                UpdateHotKey(usage_.begin(), key);
                SetExpiration(key, ttl);
            }
        }

        [[nodiscard]] bool Contains(const Key &key) const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            return cache_.find(key) != cache_.end();
        }

        [[nodiscard]] size_t Size() const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            return cache_.size();
        }

        [[nodiscard]] size_t Capacity() const {
            return capacity_;
        }

        std::vector<std::pair<Key, Value>> GetAllEntries() const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::vector<std::pair<Key, Value>> result;
            for (auto it = usage_.begin(); it != usage_.end(); ++it) {
                result.emplace_back(it->first, it->second);
            }
            return result;
        }

        std::vector<Key> GetKeys() const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::vector<Key> keys;
            for (const auto &entry: usage_) {
                keys.emplace_back(entry.first);
            }
            return keys;
        }

        std::vector<Value> GetValues() const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::vector<Value> values;
            for (const auto &entry: usage_) {
                values.emplace_back(entry.second);
            }
            return values;
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            usage_.clear();
            cache_.clear();
            access_count_.clear();
            hot_key_cache_.hot_keys.clear();
            expiration_times_.clear();
        }

        bool Remove(const Key &key) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = cache_.find(key);
            if (it == cache_.end()) {
                return false;
            }

            usage_.erase(it->second);
            cache_.erase(it);

            auto hot_it = hot_key_cache_.hot_keys.find(key);
            if (hot_it != hot_key_cache_.hot_keys.end()) {
                hot_key_cache_.hot_keys.erase(hot_it);
            }

            access_count_.erase(key);
            expiration_times_.erase(key);

            return true;
        }

        size_t BatchRemove(const std::vector<Key> &keys) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            size_t removed_count = 0;
            for (const auto &key: keys) {
                if (Remove(key)) {
                    removed_count++;
                }
            }
            return removed_count;
        }

        bool HasKey(const Key &key) const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = cache_.find(key);
            if (it == cache_.end()) return false;
            return !IsExpired(it);
        }

        std::optional<std::chrono::seconds> GetExpiryTime(const Key &key) const {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto exp_it = expiration_times_.find(key);
            if (exp_it == expiration_times_.end()) return std::nullopt;

            auto now = std::chrono::steady_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(exp_it->second - now);
            return (remaining > std::chrono::seconds::zero()) ? std::make_optional(remaining) : std::nullopt;
        }

        // Start automatic cleanup thread with specified interval
        void StartCleanupThread(std::chrono::seconds interval = std::chrono::seconds(60)) {
            std::lock_guard<std::mutex> lock(cleanup_thread_mutex_);
            if (cleanup_running_) {
                return;// Already running
            }

            cleanup_running_ = true;
            cleanup_thread_ = std::make_unique<std::thread>([this, interval]() {
                while (cleanup_running_) {
                    std::unique_lock<std::mutex> lock(cleanup_mutex_);
                    cleanup_condition_.wait_for(lock, interval, [this] { return !cleanup_running_; });

                    if (cleanup_running_) {
                        CleanUpExpiredItems();
                    }
                }
            });
        }

        // Stop automatic cleanup thread
        void StopCleanupThread() {
            std::lock_guard<std::mutex> lock(cleanup_thread_mutex_);
            if (cleanup_running_) {
                cleanup_running_ = false;
                cleanup_condition_.notify_all();

                if (cleanup_thread_ && cleanup_thread_->joinable()) {
                    cleanup_thread_->join();
                }
                cleanup_thread_.reset();
            }
        }

        // Manual cleanup of expired items
        void CleanUpExpiredItems() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            for (auto it = expiration_times_.begin(); it != expiration_times_.end();) {
                if (it->second <= clock_type::now()) {
                    // Remove the expired item
                    auto cache_it = cache_.find(it->first);
                    if (cache_it != cache_.end()) {
                        usage_.erase(cache_it->second);
                        cache_.erase(cache_it);
                    }

                    // Remove from other data structures
                    access_count_.erase(it->first);
                    hot_key_cache_.hot_keys.erase(it->first);

                    // Erase from expiration map and move iterator
                    it = expiration_times_.erase(it);
                } else {
                    ++it;
                }
            }
        }

    protected:
        void MoveToFront(typename std::unordered_map<Key, iterator>::iterator it) {
            if (usage_.begin() != it->second) {
                usage_.splice(usage_.begin(), usage_, it->second);
                cache_[it->first] = usage_.begin();
            }
        }

        void EvictLRU() {
            if (usage_.empty()) return;

            const auto &lru_entry = usage_.back();
            cache_.erase(lru_entry.first);
            expiration_times_.erase(lru_entry.first);
            access_count_.erase(lru_entry.first);
            hot_key_cache_.hot_keys.erase(lru_entry.first);
            usage_.pop_back();
        }

        void EvictLRUBatch(size_t count) {
            if (count == 0 || usage_.empty()) return;
            size_t evict_count = std::min(count, usage_.size());

            for (size_t i = 0; i < evict_count; ++i) {
                const auto &lru_entry = usage_.back();
                cache_.erase(lru_entry.first);
                expiration_times_.erase(lru_entry.first);
                access_count_.erase(lru_entry.first);
                hot_key_cache_.hot_keys.erase(lru_entry.first);
                usage_.pop_back();
            }
        }

        void EnsureCapacity(size_t required) {
            if (cache_.size() + required <= capacity_) return;

            size_t need_to_evict = cache_.size() + required - capacity_;
            EvictLRUBatch(need_to_evict);
        }

        void SetExpiration(const Key &key, std::chrono::seconds ttl) {
            if (ttl.count() > 0) {
                expiration_times_[key] = clock_type::now() + ttl;
            } else if (ttl_ > std::chrono::seconds::zero()) {
                expiration_times_[key] = clock_type::now() + ttl_;
            } else {
                expiration_times_.erase(key);
            }
        }

        void UpdateHotKey(iterator it, const Key &key) {
            access_count_[key]++;
            if (access_count_[key] >= hot_key_threshold_) {
                if (hot_key_cache_.hot_keys.find(key) == hot_key_cache_.hot_keys.end()) {
                    hot_key_cache_.hot_keys[key] = it;
                }
                access_count_.erase(key);
            }
        }

        iterator GetIterator(const Key &key) {
            auto it = cache_.find(key);
            if (it == cache_.end()) return usage_.end();
            return it->second;
        }


    private:
        bool IsExpired(const typename std::unordered_map<Key, iterator>::const_iterator &it) const {
            auto exp_it = expiration_times_.find(it->first);
            if (exp_it != expiration_times_.end()) {
                return exp_it->second <= clock_type::now();
            }
            return false;
        }

        size_t capacity_;
        size_t hot_key_threshold_;
        std::chrono::seconds ttl_;
        std::unordered_map<Key, time_point> expiration_times_;
        std::list<std::pair<Key, Value>> usage_;
        std::unordered_map<Key, iterator> cache_;
        mutable std::unordered_map<Key, size_t> access_count_;
        struct HotKeyCache {
            std::unordered_map<Key, iterator> hot_keys;
        } hot_key_cache_;

        // Thread safety
        mutable std::mutex cache_mutex_;

        // Automatic cleanup functionality
        std::unique_ptr<std::thread> cleanup_thread_;
        std::atomic<bool> cleanup_running_;
        std::mutex cleanup_mutex_;
        std::condition_variable cleanup_condition_;
        std::mutex cleanup_thread_mutex_;// Mutex to protect thread start/stop operations
    };

}// namespace Astra::datastructures