#pragma once
#include "RLruCache.hpp"
#include <memory>
#include <unordered_map>
#include <cstddef>
#include <stdexcept>

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
        if(history_capacity<=0||k<=0){
		throw std::invalid_argument("history capacity and k must be positive");
		}
		historyList_=new RLruCache<Key,std::size_t>(historyCapacity);
    }

    ~RLruKCache() override {
        delete historyList_;
    }

    Value get(const Key& key) override {
        Value value{};
        bool inMainCache = RLruCache<Key, Value>::get(key, value);
        if (inMainCache) {
            return value;
        }
		std::size_t historyCount = historyList_->get(key);
        ++historyCount;
        historyList_->put(key, historyCount);
		if(historyCount < static_cast<std::size_t>(k_)){
			return Value{};
		}

		auto iter=historyValueMap_.find(key);
		if(iter==historyValueMap_.end()){
			return Value{};
		}
		
		Value storedValue = iter->second;
		historyList_->remove(key);
		historyValueMap_.erase(iter);
		RLruCache<Key, Value>::put(key, storedValue);
		return storedValue;
    }

    void put(const Key& key, const Value& value) override {
        Value existing_value{};
		bool in_main_cache=RLruCache<Key,Value>::get(key,existing_value);

		if(in_main_cache){
			RLruCache<Key,Value>::put(key,value);
			return;
		}

		auto iter=historyValueMap_.find(key);
		if(iter!=historyValueMap_.end()){
			iter->second=value;
		}else{
			historyValueMap_[key]=value;
		}

		std::size_t history_count=historyList_->get(key);
		++history_count;
		historyList_->put(key,history_count);

		if(history_count<static_cast<std::size_t>(k_)){
			return;
		}

		auto history_iter=historyValueMap_.find(key);
		if(history_iter==historyValueMap_.end()){
			return;
		}

		Value stored_value=history_iter->second;
		historyValueMap_.erase(history_iter);
		historyList_->remove(key);
		RLruCache<Key,Value>::put(key,stored_value);
    }

    bool get(const Key& key, Value& value) override {
        Value main_value{};
		bool in_main_cache=RLruCache<Key,Value>::get(key,main_value);

		if(in_main_cache){
			value=main_value;
			return true;
		}

		std::size_t history_count=historyList_->get(key);
		++history_count;
		historyList_->put(key,history_count);

		if(history_count<static_cast<std::size_t>(k_)){
			return false;
		}

		auto history_iter=historyValueMap_.find(key);
		if(history_iter==historyValueMap_.end()){
			return false;
		}

		value=history_iter->second;
		historyValueMap_.erase(history_iter);
		historyList_->remove(key);
		RLruCache<Key,Value>::put(key,value);
		return true;
    }
};

} // namespace RrCache
