#pragma once
#include "RLruCache.hpp"
#include <memory>
#include <unordered_map>

namespace RrCache{

template<typename Key, typename Value>
class RLruKCache : public RLruCache<Key, Value>
{
private:
    int k_;
    RLruCache<Key, size_t>* historyList_;
    std::unordered_map<Key, Value> historyValueMap_;

public:
    RLruKCache(int capacity, int historyCapacity, int k)
        : RLruCache<Key, Value>(capacity), k_(k) {
        historyList_ = new RLruCache<Key, size_t>(historyCapacity);
    }

    ~RLruKCache() override {
        delete historyList_;
    }

    Value get(Key key) override {
        Value value{};
        bool inMainCache = RLruCache<Key, Value>::get(key, value);
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);
        if (inMainCache) {
            return value;
        }
        if (historyCount >= k_) {
            auto it = historyValueMap_.find(key);
            if (it != historyValueMap_.end()) {
                Value storedValue = it->second;
                historyList_->remove(key);
                historyValueMap_.erase(it);
                RLruCache<Key, Value>::put(key, storedValue);
                return storedValue;
            }
        }
        return value;
    }

    void put(Key key, Value value) override {
        Value existingValue{};
        bool inMainCache = RLruCache<Key, Value>::get(key, existingValue);
        if (inMainCache) {
            RLruCache<Key, Value>::put(key, value);
            return;
        }
        auto it = historyValueMap_.find(key);
        if (it != historyValueMap_.end()) {
            it->second = value;
        } else {
            historyValueMap_[key] = value;
        }
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);
        if (historyCount >= k_) {
            historyList_->remove(key);
            auto mapIt = historyValueMap_.find(key);
            if (mapIt != historyValueMap_.end()) {
                Value storedValue = mapIt->second;
                historyValueMap_.erase(mapIt);
                RLruCache<Key, Value>::put(key, storedValue);
            }
        }
    }

    bool get(Key key, Value& value) override {
        Value v = this->get(key);
        if (v != Value{}) {
            value = v;
            return true;
        }
        return false;
    }
};

} // namespace RrCache
