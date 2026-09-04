#pragma once
#include "cache_system.hpp"
#include <unordered_map>
#include <mutex>
#include <queue>
#include <memory>
#include <cstddef>

namespace RrCache{

    template<typename Key, typename Value>
    class RFIFOCache : public cache_system<Key, Value>{
    private:
        int capacity_;
        std::unordered_map<Key, Value> cache_;
        std::queue<Key> orderQueue_;
        std::mutex mutex_;

        void evict(){
            Key oldestKey = orderQueue_.front();
            orderQueue_.pop();
            cache_.erase(oldestKey);
        }

    public:
        explicit RFIFOCache(int capacity) : capacity_(capacity){}
        
        ~RFIFOCache() override = default;

        void put(const Key& key, const Value& value) override{
            if(capacity_ <= 0)
                return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = cache_.find(key);
            if(it != cache_.end()){
                it->second = value;
                return;
            }
            
            if(cache_.size() >= static_cast<std::size_t>(capacity_)){
                evict();
            }
            
            cache_[key] = value;
            orderQueue_.push(key);
        }

        bool get(const Key& key, Value& value) override{
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(key);
            if(it == cache_.end()){
				return false;
            }
            value=it->second;
			return true;
        }
        Value get(const Key& key) override{
            Value value{};
            get(key, value);
            return value;
        }
    };
}

