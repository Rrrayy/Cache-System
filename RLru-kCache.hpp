#pragma once
#include"RLruCache.hpp"
#include<memory>
namespace RrCache{

//LRU-k LRU优化版:数据被访问k次才可以进入缓存
    template<typename Key,typename Value>
    class RLruKCache:public RLruCache<Key,Value>
    {
        private:
            int k_;//进入缓存的评判标准，如果数据被访问k次，则可进入缓存
            std::unique_ptr<RLruKCache<Key,size_t>>historyList_;//访问数据历史记录
            std::unordered_map<Key,Value>historyValueMap_;//存储未达到k次访问的数据值
        public:
            RLruKCache(int capacity,int historyCapacity,int k)
                :RLruCache<Key,Value>(capacity)
                ,historyList_(std::make_unique<RLruKCache<Key,size_t>>(historyCapacity))
                ,k_(k){}
            Value get(Key key){
                Value value{};
                bool inMainCache=RLruCache<Key,Value>::get(key,value);
                size_t historyCount=historyList_->get(key);
                historyCount++;
                historyList_->put(key,historyCount);
                if(inMainCache){
                    return value;
                }
                if(historyCount>=k){
                    auto it =historyValueMap_.find(key);
                    if(it!=historyValueMap_.end()){
                        Value storedValue=it->second;
                        historyList_->remove(key);
                        historyValueMap_.erase(it);
                        RLruCache<Key,Value>::put(key,storedValue);
                        return storedValue;
                    }
                }
                return value;
            }
            void put(Key key,Value value){
                Value existingValue{};
                bool inMainCache=RLruCache<Key,Value>::get(key,existingValue);
                if(inMainCache){
                    //已经在主缓存，直接更新
                    RLruCache<Key,Value>::put(key,value);
                    return;
                }
                //获取并更新访问历史
                size_t historyCount=historyList_->get(key);
                historyCount++;
                historyList_->put(key,historyCount);
                //保存值到历史记录映射，供后续get操作使用
                historyValueMap_[key]=value;
                //检查访问次数是否到达K
                if(historyCount>=k){
                    historyList_->remove(key);
                    historyValueMap_.erase(key);
                    RLruCache<Key,Value>::put(key,value);
                }

            }
    };
}