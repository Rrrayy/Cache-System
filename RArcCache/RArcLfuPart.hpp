#pragma once

#include"RArcCacheNode.hpp"

#include<cstddef>
#include<list>
#include<memory>
#include<mutex>
#include<unordered_map>

namespace RrCache{

template<typename Key,typename Value>
class ArcLruPart{
public:
	using NodeType=ArcNode<Key,Value>;
	using NodePtr=std::shared_ptr<NodeType>;
	using NodeMap=std::unordered_map<Key,NodePtr>;
	using GhostList=std::list<Key>;
	using GhostMap=std::unordered_map<
		Key,
		typename GhostList::iterator
	>;

	ArcLruPart(
		std::size_t capacity,
		std::size_t transformThreshold,
		std::size_t ghostCapacity=0)
		:capacity_(capacity),
		ghostCapacity_(
			ghostCapacity==0
				?capacity
				:ghostCapacity),
		transformThreshold_(
			transformThreshold==0
				?1
				:transformThreshold){
		initializeLists();
	}

	bool put(const Key& key,const Value& value){
		std::lock_guard<std::mutex> lock(mutex_);

		if(capacity_==0){
			return false;
		}

		auto iter=mainCache_.find(key);
		if(iter!=mainCache_.end()){
			iter->second->setValue(value);
			moveToFront(iter->second);
			return true;
		}

		while(mainCache_.size()>=capacity_){
			if(!evictLeastRecent()){
				return false;
			}
		}

		NodePtr newNode=
			std::make_shared<NodeType>(key,value);

		mainCache_[key]=newNode;
		addToFront(newNode);
		return true;
	}

	bool get(
		const Key& key,
		Value& value,
		bool& shouldTransform){
		std::lock_guard<std::mutex> lock(mutex_);

		auto iter=mainCache_.find(key);
		if(iter==mainCache_.end()){
			shouldTransform=false;
			return false;
		}

		NodePtr node=iter->second;
		node->incrementAccessCount();
		moveToFront(node);

		value=node->getValue();
		shouldTransform=
			node->getAccessCount()>=transformThreshold_;
		return true;
	}

	bool contain(const Key& key){
		std::lock_guard<std::mutex> lock(mutex_);
		return mainCache_.find(key)!=mainCache_.end();
	}

	bool remove(const Key& key){
		std::lock_guard<std::mutex> lock(mutex_);

		auto iter=mainCache_.find(key);
		if(iter==mainCache_.end()){
			return false;
		}

		removeFromMain(iter->second);
		mainCache_.erase(iter);
		return true;
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
			if(!evictLeastRecent()){
				break;
			}
		}

		return true;
	}

private:
	void initializeLists(){
		mainHead_=std::make_shared<NodeType>();
		mainTail_=std::make_shared<NodeType>();

		mainHead_->next_=mainTail_;
		mainTail_->prev_=mainHead_;
	}

	void addToFront(const NodePtr& node){
		NodePtr firstNode=mainHead_->next_;

		node->prev_=mainHead_;
		node->next_=firstNode;
		firstNode->prev_=node;
		mainHead_->next_=node;
	}

	void removeFromMain(const NodePtr& node){
		if(!node){
			return;
		}

		NodePtr previousNode=node->prev_.lock();
		NodePtr nextNode=node->next_;

		if(!previousNode||!nextNode){
			return;
		}

		previousNode->next_=nextNode;
		nextNode->prev_=previousNode;
		node->prev_.reset();
		node->next_.reset();
	}

	void moveToFront(const NodePtr& node){
		removeFromMain(node);
		addToFront(node);
	}

	bool evictLeastRecent(){
		NodePtr leastRecent=mainTail_->prev_.lock();

		if(!leastRecent||leastRecent==mainHead_){
			return false;
		}

		Key key=leastRecent->getKey();
		removeFromMain(leastRecent);
		mainCache_.erase(key);
		addToGhost(key);
		return true;
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
	std::mutex mutex_;

	NodeMap mainCache_;
	GhostList ghostList_;
	GhostMap ghostCache_;

	NodePtr mainHead_;
	NodePtr mainTail_;
};

}