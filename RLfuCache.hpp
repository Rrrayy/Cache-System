#pragma once

/**
 * @file RLFUCache.hpp
 * @brief LFU（最不经常使用）缓存实现
 * 
 * @description
 * LFU（Least Frequently Used）缓存淘汰策略：
 * - 每个数据维护一个访问频次（freq）
 * - 当缓存满时，淘汰访问频次最低的数据
 * - 如果频次相同，淘汰最早进入该频次链表的数据
 * 
 * 数据结构设计：
 * - FreqList：按访问频次组织的双向链表，同一频次的数据串在一起
 * - nodeMap_：Key 到节点的哈希表，实现 O(1) 查找
 * - freqToFreqList_：频次到链表的映射，实现 O(1) 找到最低频次
 * 
 * 频次衰减机制：
 * - 当平均访问频次超过上限时，所有节点频次衰减
 * - 防止热点数据长期占据缓存，给冷数据机会进入缓存
 * 
 * 时间复杂度：
 * - get：O(1)
 * - put：O(1)
 * 
 * @tparam Key   缓存键类型，需要支持 std::hash
 * @tparam Value 缓存值类型
 */
 #include <algorithm>
#include <climits>
 #include <memory>
 #include <cmath>
 #include <mutex>
 #include <unordered_map>

 #include "cache_system.hpp"


 namespace RrCache{
    template<typename Key,typename Value> class RLfuCache;
    template<typename Key,typename Value>
    class FreqList{
    private:

        struct Node{
            int freq;//访问频次
            Key key;
            Value value;
            std::weak_ptr<Node> pre;//上一结点为weak_ptr打破循环引用
            std::shared_ptr<Node>next;
            Node():freq(1),next(nullptr){}
            Node(Key key,Value value)
            :freq(1),key(key),value(value),next(nullptr){}
        };

        using NodePtr=std::shared_ptr<Node>;
        int freq_;//访问频率
        NodePtr head_;
        NodePtr tail_;

    public:

        explicit FreqList(int n):freq_(n){
            head_=std::make_shared<Node>();
            tail_=std::make_shared<Node>();
            head_->next=tail_;
            tail_->pre=head_;
        }

        bool isEmpty() const{
            return head_->next==tail_;
        }

        void addNode(NodePtr node){
            if(!node||!head_||!tail_)
                return ;
            node->pre=tail_->pre;
            node->next=tail_;
            tail_->pre.lock()->next=node;
            tail_->pre=node;
        }

        void removeNode(NodePtr node){
            if(!node||!tail_||!head_)
                return;
            if(node->pre.expired()||!node->next)
                return;
            auto pre=node->pre.lock();
            pre->next=node->next;
            node->next->pre=pre;
            node->next=nullptr;//显式置空next指针，断开结点和链表的连接
        }

        NodePtr getFirstNode() const {return head_->next;}

        friend class RLfuCache<Key,Value>;
    };

    template <typename Key,typename Value>
    class RLfuCache : public cache_system<Key,Value>{ 

    public:

        using Node =typename FreqList<Key,Value>::Node;
        using NodePtr=std::shared_ptr<Node>;
        using NodeMap=std::unordered_map<Key,NodePtr>;

    private:

        void putInternal(Key key ,Value value); //添加缓存
        void getInternal(NodePtr node,Value &value);//获取缓存   
        void kickOut();//移除缓存中的过期数据
        void removeFromFreqList(NodePtr node);//从频率列表中移除节点
        void addToFreqList(NodePtr node);//添加到频率列表
        void addFreqNum();//添加平均访问等频率
        void decreaseFreqNum(int num);//减少平均访问等频率
        void handleOverMaxAverageNum();//处理当前平均访问频率超过上限的情况
        void updateMinFreq();

    private:

        int capacity_;//缓存容量
        int minFreq_;//最小访问频次（用于找到最小访问频次的节点）
        int maxAverageNum_;//最大平均访问频次
        int curAverageNUm_;//当前平均访问频次
        int curTotalNum_;//当前访问所有缓存次数总数
        std::mutex mutex_;
        NodeMap nodeMap_;//key到结点的映射
        std::unordered_map<int,FreqList<Key,Value>*>freqToFreqList_;//访问频次到频次链表的映射

    public:

        RLfuCache(int capacity,int maxAverageNum=1000000)
        :capacity_(capacity),minFreq_(INT_MAX),maxAverageNum_(maxAverageNum),
        curAverageNUm_(0),curTotalNum_(0){}
        
        ~RLfuCache() override=default;

        void put(Key key,Value value) override{
            if(capacity_==0)
                return;
            std::lock_guard<std::mutex>lock(mutex_);
            auto it =nodeMap_.find(key);
            if(it!=nodeMap_.end()){
                it->second->value=value;
                getInternal(it->second,value);
                return ;
            }
            putInternal(key,value);
        }

        bool get(Key key,Value &value) override{
            std::lock_guard<std::mutex>lock(mutex_);
            auto it=nodeMap_.find(key);
            if(it!=nodeMap_.end()){
                getInternal(it->second,value);
                return true;
            }
            return false;
        }

        Value get(Key key) override{
            Value value{};
            get(key,value);
            return value;
        }
        
        //清空缓存，避免内存泄漏
        void purge(){
            for (auto& pair : freqToFreqList_){
                delete pair.second;
            }
            nodeMap_.clear();
            freqToFreqList_.clear();
        }
    };

    template<typename Key,typename Value>
    void RLfuCache<Key,Value>::getInternal(NodePtr node,Value &value){
        //因为该函数访问增加访问频次，所以找到之后需要将其从当前访问频次列表删除，并且添加到当前freq+1链表
        value=node->value;
        //从原有访问频次链表中删除节点
        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);
        //如果当前访问频次等于minFreq+1,并且前驱minFreq链表为空
        //则说明freqToFreqList_[node->freq-1]链表的node因被访问而迁移，需要更新最小访问频次
        if(node->freq-1==minFreq_&&freqToFreqList_[node->freq-1]->isEmpty()){
            minFreq_++;
        }
        //总访问频次和当前平均访问频次都随之增加
        addFreqNum();
    }

    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::putInternal(Key key,Value value){
        //如果不在缓存中，则需要判断缓存是否已满
        if(nodeMap_.size()==capacity_){
            //如果缓存已满，删除最不经常访问结点，更新当前平均访问频次和总访问频次
            kickOut();
        }
        //创建新结点，将新结点添加进入，更新最小访问频次
        NodePtr node=std::make_shared<Node>(key,value);
        nodeMap_[key]=node;
        addToFreqList(node);
        addFreqNum();
        minFreq_=std::min(minFreq_,1);
    }

    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::kickOut(){
        NodePtr node=freqToFreqList_[minFreq_]->getFirstNode();
        removeFromFreqList(node);
        nodeMap_.erase(node->key);
        decreaseFreqNum(node->freq);
    }

    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::removeFromFreqList(NodePtr node){
        if(!node)
            return;
        auto freq=node->freq;
        freqToFreqList_[freq]->removeNode(node);
    }

    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::addToFreqList(NodePtr node){
        if(!node)
            return ;
        // 添加进入相应的频次链表前需要判断该频次链表是否存在
        auto freq=node->freq;
        if(freqToFreqList_.find(node->freq)==freqToFreqList_.end()){
            //不存在则创建
            freqToFreqList_[node->freq]=new FreqList<Key, Value>(node->freq);
        }
        freqToFreqList_[freq]->addNode(node);
    }

    template<typename Key,typename Value>
    void RLfuCache<Key,Value>::addFreqNum(){
        curTotalNum_++;
        if(nodeMap_.empty())
            curAverageNUm_=0;
        else
            curAverageNUm_=curTotalNum_/nodeMap_.size();
        
        if(curAverageNUm_>maxAverageNum_){
            handleOverMaxAverageNum();
        }
    }

    template<typename Key,typename Value>
    void RLfuCache<Key,Value>::decreaseFreqNum(int num){
        curTotalNum_-=num;
        if(nodeMap_.empty())   
            curAverageNUm_=0;
        else 
            curAverageNUm_=curTotalNum_/nodeMap_.size();
    }

    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::handleOverMaxAverageNum(){
        if(nodeMap_.empty())
            return;
        //该函数为当前平均访问频次已经超过了最大平均访问频次，所以所有节点的 访问频次-（maxAverageNum_/2）
        for(auto it =nodeMap_.begin();it!=nodeMap_.end();++it){
            if(!it->second)
                continue;
            NodePtr node=it->second;
            //从当前频率列表中移除
            removeFromFreqList(node);
            int oldFreq=node->freq;
            int decay=maxAverageNum_/2;
            node->freq-=decay;
            if(node->freq<1)
                node->freq=1;
            int delta=node->freq-oldFreq;
            curTotalNum_+=delta;

            addToFreqList(node);
        }
        updateMinFreq();
    }
    template<typename Key,typename Value>
    void RLfuCache<Key, Value>::updateMinFreq(){
        minFreq_=INT_MAX;
        for(const auto&pair:freqToFreqList_){
            if(pair.second&&!pair.second->isEmpty()){
                minFreq_=std::min(minFreq_,pair.first);
            }
        }
        if(minFreq_==INT_MAX)
            minFreq_=1;
    }
 }

    