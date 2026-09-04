#pragma once

#include"RArcCacheNode.hpp"

#include<cstddef>
#include<list>
#include<map>
#include<memory>
#include<mutex>
#include<unordered_map>

namespace RrCache{

template<typename Key,typename Value>
class ArcLfuPart{
public:
	using NodeType=ArcNode<Key,Value>;
	using NodePtr=std::shared_ptr<NodeType>;
	using NodeMap=std::unordered_map<Key,NodePtr>;
	using NodeList=std::list<NodePtr>;
	using FreqMap=std::map<std::size_t,NodeList>;
	using PositionMap=std::unordered_map<
		Key,
		typename NodeList::iterator
	>;
	using GhostList=std::list<Key>;
	using GhostMap=std::unordered_map<
		Key,
		typename GhostList::iterator
	>;

	ArcLfuPart(
		std::size_t capacity,
		std::size_t transformThreshold,
		std::size_t ghostCapacity=0)
		:capacity_(capacity),
		ghostCapacity_(
			ghostCapacity==0
				?capacity
				:ghostCapacity),
		transformThreshold_(transformThreshold),
		minFreq_(0){}

	bool put(const Key& key,const Value& value){
		std::lock_guard<std::mutex> lock(mutex_);

		if(capacity_==0){
			return false;
		}

		auto iter=mainCache_.find(key);
		if(iter!=mainCache_.end()){
			iter->second->setValue(value);
			updateNodeFrequency(iter->second);
			return true;
		}

		while(mainCache_.size()>=capacity_){
			if(!evictLeastFrequent()){
				return false;
			}
		}

		NodePtr newNode=
			std::make_shared<NodeType>(key,value);

		mainCache_[key]=newNode;
		freqMap_[1].push_back(newNode);
		nodePositions_[key]=
			std::prev(freqMap_[1].end());
		minFreq_=1;
		return true;
	}

	bool get(const Key& key,Value& value){
		std::lock_guard<std::mutex> lock(mutex_);

		auto iter=mainCache_.find(key);
		if(iter==mainCache_.end()){
			return false;
		}

		NodePtr node=iter->second;
		value=node->getValue();
		updateNodeFrequency(node);
		return true;
	}

	bool contain(const Key& key){
		std::lock_guard<std::mutex> lock(mutex_);
		return mainCache_.find(key)!=mainCache_.end();
	}

	bool checkGhost(const Key& key){
		std::lock_guard<std::mutex> lock(mutex_);

		auto iter=ghostCache_.find(key);
		if(iter==ghostCache_.end()){
			return false;
		}

		ghostList_.erase(iter->second);
		ghostCache_.erase(iter);
		return true;
	}

	void increaseCapacity(){
		std::lock_guard<std::mutex> lock(mutex_);
		++capacity_;
	}

	bool decreaseCapacity(){
		std::lock_guard<std::mutex> lock(mutex_);

		if(capacity_==0){
			return false;
		}

		--capacity_;

		while(mainCache_.size()>capacity_){
			if(!evictLeastFrequent()){
				break;
			}
		}

		return true;
	}

private:
	void updateNodeFrequency(const NodePtr& node){
		std::size_t oldFreq=node->getAccessCount();

		auto listIter=freqMap_.find(oldFreq);
		auto positionIter=nodePositions_.find(node->getKey());

		if(listIter!=freqMap_.end()&&
			positionIter!=nodePositions_.end()){
			listIter->second.erase(positionIter->second);

			if(listIter->second.empty()){
				freqMap_.erase(listIter);
			}
		}

		node->incrementAccessCount();
		std::size_t newFreq=node->getAccessCount();

		freqMap_[newFreq].push_back(node);
		nodePositions_[node->getKey()]=
			std::prev(freqMap_[newFreq].end());

		updateMinFreq();
	}

	bool evictLeastFrequent(){
		if(freqMap_.empty()){
			return false;
		}

		auto freqIter=freqMap_.begin();
		NodeList& nodeList=freqIter->second;

		if(nodeList.empty()){
			freqMap_.erase(freqIter);
			updateMinFreq();
			return false;
		}

		NodePtr leastNode=nodeList.front();
		Key key=leastNode->getKey();

		nodeList.pop_front();
		if(nodeList.empty()){
			freqMap_.erase(freqIter);
		}

		nodePositions_.erase(key);
		mainCache_.erase(key);
		addToGhost(key);
		updateMinFreq();
		return true;
	}

	void updateMinFreq(){
		if(freqMap_.empty()){
			minFreq_=0;
			return;
		}

		minFreq_=freqMap_.begin()->first;
	}

	void addToGhost(const Key& key){
		if(ghostCapacity_==0){
			return;
		}

		auto existingIter=ghostCache_.find(key);
		if(existingIter!=ghostCache_.end()){
			ghostList_.erase(existingIter->second);
			ghostCache_.erase(existingIter);
		}

		while(ghostCache_.size()>=ghostCapacity_){
			removeOldestGhost();
		}

		ghostList_.push_front(key);
		ghostCache_[key]=ghostList_.begin();
	}

	void removeOldestGhost(){
		if(ghostList_.empty()){
			return;
		}

		const Key& key=ghostList_.back();
		ghostCache_.erase(key);
		ghostList_.pop_back();
	}

private:
	std::size_t capacity_;
	std::size_t ghostCapacity_;
	std::size_t transformThreshold_;
	std::size_t minFreq_;
	std::mutex mutex_;

	NodeMap mainCache_;
	FreqMap freqMap_;
	PositionMap nodePositions_;

	GhostList ghostList_;
	GhostMap ghostCache_;
};

}