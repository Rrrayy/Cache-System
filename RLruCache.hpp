#pragma once
#include"cache_system.hpp"
#include"LruNode.hpp"
#include<unordered_map>
#include<mutex>
#include<cstring>
#include<memory>
#include<list>
namespace RrCache{

    template <typename Key,typename Value>
    class RLruCache: public cache_system<Key,Value>{
        
        private:
        //改写1，将类型别名写在private里
        using LruNodeType=LruNode<Key,Value>;
        using NodePtr=std::shared_ptr<LruNode<Key,Value>>;
        using NodeMap=std::unordered_map<Key,NodePtr>;
        int capacity_;//缓存容量
        NodeMap nodeMap_;//key->node
        std::mutex mutex_;//防止多线程同时访问同一数据
        NodePtr dummyHead_;//虚拟头结点
        NodePtr dummyTail_;

        private:
        void initializeList(){
            dummyHead_=std::make_shared<LruNodeType>(Key(),Value());
            dummyTail_=std::make_shared<LruNodeType>(Key(),Value());
            dummyHead_->getNext()=dummyTail_;
            dummyTail_->getPrev()=dummyHead_;
        }
        //删除结点
        void removeNode(NodePtr node){
            if(!node->getPrev().expired()&&node->getNext()){
                auto prev = node->getPrev().lock();
                prev->getNext()=node->getNext();
                node->getNext()->getPrev()=prev;
                node->getNext()=nullptr;
            }
        }
        //从尾部插入节点
        void insertNode(NodePtr node){
            node->getNext()=dummyTail_;
            node->getPrev()=dummyTail_->getPrev();
            dummyTail_->getPrev().lock()->getNext()=node;
            dummyTail_->getPrev()=node;
        }
        //驱逐最近最少访问
        void evictLeastRecent(){
            NodePtr leastRecent=dummyHead_->getNext();
            removeNode(leastRecent);
            nodeMap_.erase(leastRecent->getKey());
        }
        //将表中访问的最新结点移动到最新位置
        void moveToMostRecent(NodePtr node){
            removeNode(node);
            insertNode(node);
        }
        //添加新结点
        void addNewNode(const Key &key,const Value &value){
            if(nodeMap_.size()>=capacity_){
                evictLeastRecent();
            }
            NodePtr newNode=std::make_shared<LruNodeType>(key,value);
            insertNode(newNode);
            nodeMap_[key]=newNode;
        }
        //更新结点，并根据lru策略将结点移动到链表最新被访问位置
        void updateExistingNode(NodePtr node,const Value&value){
            node->setValue(value);
            moveToMostRecent(node);
        }

        public:
        
        RLruCache(int capacity):capacity_(capacity){
            initializeList();
        }
        ~RLruCache() override=default;
        //添加缓存
        void put(Key key,Value value)override{
            if(capacity_<=0)
                return;
            std::lock_guard<std::mutex> lock(mutex_);
            auto it=nodeMap_.find(key);
            if(it!=nodeMap_.end()){
                updateExistingNode(it->second,value);
                return;
            }
            addNewNode(key,value);
        }
        bool get(Key key,Value &value)override{
            std::lock_guard<std::mutex>lock(mutex_);
            auto it=nodeMap_.find(key);
            if(it!=nodeMap_.end()){
                moveToMostRecent(it->second);
                value=it->second->getValue();
                return true;
            }
            return false;
        }
        Value get(Key key)override{
            Value value={};
            get(key,value);
            return value;
        }
        //删除指定元素
        void remove(Key key){
            std::lock_guard<std::mutex>lock(mutex_);
            auto it=nodeMap_.find(key);
            if(it!=nodeMap_.end()){
                removeNode(it->second);
                nodeMap_.erase(it);
            }
        }
    };
}