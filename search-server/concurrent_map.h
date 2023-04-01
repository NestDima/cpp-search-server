#pragma once
#include <map>
#include <vector>
#include <mutex>

#include "log_duration.h"

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct MapMutex {
        std::mutex mutex_;
        std::map<Key, Value> map_;
    };

    std::vector<MapMutex> map_mutex_;

public:
    struct Access {
        std::lock_guard<std::mutex> lock_guard_mutex;
        Value& value_reference;

    Access(const Key& key, MapMutex& map_mutex)
        : lock_guard_mutex(map_mutex.mutex_)
        , value_reference(map_mutex.map_[key]) {}
    };

    explicit ConcurrentMap(size_t bucket_count)
        : map_mutex_(bucket_count) {}

    Access operator[](const Key& key) {
        auto& bucket = map_mutex_[uint64_t(key) % map_mutex_.size()];
        return {key, bucket};
    }

    std::map<Key, Value> BuildMap() {
        std::map<Key, Value> result;
        for (auto& [mutex_, map_] : map_mutex_) {
            std::lock_guard lock_guard_mutex(mutex_);
            result.insert(map_.begin(), map_.end());
        }
        return result;
    }

    void Erase(const Key& key){
      auto& map_mutex = map_mutex_[static_cast<uint64_t>(key) % map_mutex_.size()];
        std::lock_guard lock_guard_mutex(map_mutex.mutex_);
        map_mutex.map_.erase(key);
    }
};
