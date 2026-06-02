#pragma once
#include "cache_system.hpp"
#include <unordered_map>
#include <mutex>
#include <queue>
#include <memory>

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

        void put(Key key, Value value) override{
            if(capacity_ <= 0)
                return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = cache_.find(key);
            if(it != cache_.end()){
                it->second = value;
                return;
            }
            
            if(cache_.size() >= capacity_){
                evict();
            }
            
            cache_[key] = value;
            orderQueue_.push(key);
        }

        bool get(Key key, Value &value) override{
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(key);
            if(it != cache_.end()){
                value = it->second;
                return true;
            }
            return false;
        }
        Value get(Key key) override{
            Value value{};
            get(key, value);
            return value;
        }
    };
}

