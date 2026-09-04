#pragma once

#include<algorithm>
#include<climits>
#include<cstddef>
#include<memory>
#include<mutex>
#include<unordered_map>

#include"cache_system.hpp"

namespace RrCache{

template<typename Key,typename Value>
class RLfuCache;

template<typename Key,typename Value>
class FreqList{
private:
	struct Node{
		int freq;
		Key key;
		Value value;
		std::weak_ptr<Node> pre;
		std::shared_ptr<Node> next;

		Node()
			:freq(1),next(nullptr){}

		Node(const Key& key,const Value& value)
			:freq(1),key(key),value(value),next(nullptr){}
	};

	using NodePtr=std::shared_ptr<Node>;

	int freq_;
	NodePtr head_;
	NodePtr tail_;

public:
	explicit FreqList(int n)
		:freq_(n){
		head_=std::make_shared<Node>();
		tail_=std::make_shared<Node>();
		head_->next=tail_;
		tail_->pre=head_;
	}

	bool isEmpty() const{
		return head_->next==tail_;
	}

	void addNode(const NodePtr& node){
		if(!node||!head_||!tail_){
			return;
		}

		auto previous_node=tail_->pre.lock();
		if(!previous_node){
			return;
		}

		node->pre=previous_node;
		node->next=tail_;
		previous_node->next=node;
		tail_->pre=node;
	}

	void removeNode(const NodePtr& node){
		if(!node||!head_||!tail_){
			return;
		}

		auto previous_node=node->pre.lock();
		auto next_node=node->next;

		if(!previous_node||!next_node){
			return;
		}

		previous_node->next=next_node;
		next_node->pre=previous_node;
		node->pre.reset();
		node->next.reset();
	}

	NodePtr getFirstNode() const{
		if(isEmpty()){
			return nullptr;
		}

		return head_->next;
	}

	friend class RLfuCache<Key,Value>;
};

template<typename Key,typename Value>
class RLfuCache:public cache_system<Key,Value>{
public:
	using Node=typename FreqList<Key,Value>::Node;
	using NodePtr=std::shared_ptr<Node>;
	using NodeMap=std::unordered_map<Key,NodePtr>;

private:
	void putInternal(const Key& key,const Value& value);
	void getInternal(const NodePtr& node,Value& value);
	void kickOut();
	void removeFromFreqList(const NodePtr& node);
	void addToFreqList(const NodePtr& node);
	void addFreqNum();
	void decreaseFreqNum(int num);
	void handleOverMaxAverageNum();
	void updateMinFreq();
	void purgeInternal();

private:
	int capacity_;
	int minFreq_;
	int maxAverageNum_;
	int curAverageNUm_;
	int curTotalNum_;
	std::mutex mutex_;
	NodeMap nodeMap_;
	std::unordered_map<int,FreqList<Key,Value>*> freqToFreqList_;

public:
	RLfuCache(int capacity,int maxAverageNum=1000000)
		:capacity_(capacity),
		minFreq_(INT_MAX),
		maxAverageNum_(std::max(1,maxAverageNum)),
		curAverageNUm_(0),
		curTotalNum_(0){}

	~RLfuCache() override{
		purgeInternal();
	}

	void put(const Key& key,const Value& value) override{
		if(capacity_<=0){
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		auto iter=nodeMap_.find(key);

		if(iter!=nodeMap_.end()){
			iter->second->value=value;

			Value cached_value{};
			getInternal(iter->second,cached_value);
			return;
		}

		putInternal(key,value);
	}

	bool get(const Key& key,Value& value) override{
		std::lock_guard<std::mutex> lock(mutex_);
		auto iter=nodeMap_.find(key);

		if(iter==nodeMap_.end()){
			return false;
		}

		getInternal(iter->second,value);
		return true;
	}

	Value get(const Key& key) override{
		Value value{};
		get(key,value);
		return value;
	}

	void purge(){
		std::lock_guard<std::mutex> lock(mutex_);
		purgeInternal();
	}
};

template<typename Key,typename Value>
void RLfuCache<Key,Value>::getInternal(
	const NodePtr& node,
	Value& value){
	if(!node){
		return;
	}

	value=node->value;
	int old_freq=node->freq;

	removeFromFreqList(node);
	++node->freq;
	addToFreqList(node);

	auto old_list_iter=freqToFreqList_.find(old_freq);
	if(old_freq==minFreq_&&
		old_list_iter!=freqToFreqList_.end()&&
		old_list_iter->second!=nullptr&&
		old_list_iter->second->isEmpty()){
		++minFreq_;
	}

	addFreqNum();
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::putInternal(
	const Key& key,
	const Value& value){
	if(nodeMap_.size()>=static_cast<std::size_t>(capacity_)){
		kickOut();
	}

	NodePtr node=std::make_shared<Node>(key,value);
	nodeMap_[key]=node;
	addToFreqList(node);

	minFreq_=1;
	addFreqNum();
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::kickOut(){
	auto list_iter=freqToFreqList_.find(minFreq_);

	if(list_iter==freqToFreqList_.end()||
		list_iter->second==nullptr||
		list_iter->second->isEmpty()){
		updateMinFreq();
		list_iter=freqToFreqList_.find(minFreq_);
	}

	if(list_iter==freqToFreqList_.end()||
		list_iter->second==nullptr||
		list_iter->second->isEmpty()){
		return;
	}

	NodePtr node=list_iter->second->getFirstNode();
	if(!node){
		return;
	}

	removeFromFreqList(node);
	nodeMap_.erase(node->key);
	decreaseFreqNum(node->freq);
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::removeFromFreqList(
	const NodePtr& node){
	if(!node){
		return;
	}

	auto list_iter=freqToFreqList_.find(node->freq);
	if(list_iter==freqToFreqList_.end()||
		list_iter->second==nullptr){
		return;
	}

	list_iter->second->removeNode(node);
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::addToFreqList(
	const NodePtr& node){
	if(!node){
		return;
	}

	int freq=node->freq;
	auto list_iter=freqToFreqList_.find(freq);

	if(list_iter==freqToFreqList_.end()){
		freqToFreqList_[freq]=new FreqList<Key,Value>(freq);
	}

	freqToFreqList_[freq]->addNode(node);
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::addFreqNum(){
	++curTotalNum_;

	if(nodeMap_.empty()){
		curAverageNUm_=0;
	}else{
		curAverageNUm_=
			curTotalNum_/static_cast<int>(nodeMap_.size());
	}

	if(curAverageNUm_>maxAverageNum_){
		handleOverMaxAverageNum();
	}
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::decreaseFreqNum(int num){
	curTotalNum_-=num;

	if(curTotalNum_<0){
		curTotalNum_=0;
	}

	if(nodeMap_.empty()){
		curAverageNUm_=0;
	}else{
		curAverageNUm_=
			curTotalNum_/static_cast<int>(nodeMap_.size());
	}
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::handleOverMaxAverageNum(){
	if(nodeMap_.empty()){
		return;
	}

	int decay=std::max(1,maxAverageNum_/2);

	for(auto iter=nodeMap_.begin();iter!=nodeMap_.end();++iter){
		NodePtr node=iter->second;
		if(!node){
			continue;
		}

		removeFromFreqList(node);

		int old_freq=node->freq;
		node->freq=std::max(1,node->freq-decay);
		curTotalNum_+=node->freq-old_freq;

		addToFreqList(node);
	}

	if(curTotalNum_<0){
		curTotalNum_=0;
	}

	curAverageNUm_=
		curTotalNum_/static_cast<int>(nodeMap_.size());

	updateMinFreq();
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::updateMinFreq(){
	minFreq_=INT_MAX;

	for(const auto& pair:freqToFreqList_){
		if(pair.second!=nullptr&&!pair.second->isEmpty()){
			minFreq_=std::min(minFreq_,pair.first);
		}
	}

	if(minFreq_==INT_MAX){
		minFreq_=1;
	}
}

template<typename Key,typename Value>
void RLfuCache<Key,Value>::purgeInternal(){
	for(auto& pair:freqToFreqList_){
		delete pair.second;
		pair.second=nullptr;
	}

	nodeMap_.clear();
	freqToFreqList_.clear();
	minFreq_=INT_MAX;
	curAverageNUm_=0;
	curTotalNum_=0;
}

}