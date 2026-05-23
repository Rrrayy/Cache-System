#pragma once
#include<memory>
#include<cstddef>
namespace RrCache{

    template<typename Key,typename Value>
    class LruNode{

    private:
        Key key_;
        Value value_;
        size_t accesscount_;//访问次数
        std::weak_ptr<LruNode<Key,Value>>prev_;//用weak_ptr打破循环引用
        std::shared_ptr<LruNode<Key,Value>>next_;

    public:
        LruNode(Key key,Value value):key_(key),value_(value),accesscount_(1){}
        //提供访问器
        Key getKey() const{return key_;}
        Value getValue() const {return value_;}
        void setValue(const Value &value){value_=value;}
        size_t getAccessCount()const{return accesscount_;}
        void incrementAccessCount(){accesscount_++;}
         

    };
}