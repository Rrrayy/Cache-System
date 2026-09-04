#pragma once

#include"../cache_system.hpp"
#include"RArcLfuPart.hpp"
#include"RArcLruPart.hpp"

#include<cstddef>
#include<memory>
#include<mutex>

namespace RrCache{

template<typename Key,typename Value>
class RArcCache:public cache_system<Key,Value>{
public:
	explicit RArcCache(
		std::size_t capacity=10,
		std::size_t transformThreshold=2)
		:capacity_(capacity),
		transformThreshold_(
			transformThreshold==0
				?1
				:transformThreshold),
		lruPart_(
			std::make_unique<ArcLruPart<Key,Value>>(
				(capacity+1)/2,
				transformThreshold_,
				capacity)),
		lfuPart_(
			std::make_unique<ArcLfuPart<Key,Value>>(
				capacity/2,
				transformThreshold_,
				capacity)){}

	~RArcCache() override=default;

	void put(
		const Key& key,
		const Value& value) override{
		std::lock_guard<std::mutex> lock(mutex_);

		if(capacity_==0){
			return;
		}

		checkGhostCaches(key);

		if(lfuPart_->contain(key)){
			lfuPart_->put(key,value);
			return;
		}

		if(lruPart_->put(key,value)){
			return;
		}

		lfuPart_->put(key,value);
	}

	bool get(
		const Key& key,
		Value& value) override{
		std::lock_guard<std::mutex> lock(mutex_);

		if(capacity_==0){
			return false;
		}

		checkGhostCaches(key);

		bool shouldTransform=false;

		if(lruPart_->get(
			key,
			value,
			shouldTransform)){
			if(shouldTransform&&
				lfuPart_->put(key,value)){
				lruPart_->remove(key);
			}

			return true;
		}

		return lfuPart_->get(key,value);
	}

	Value get(const Key& key) override{
		Value value{};
		get(key,value);
		return value;
	}

private:
	bool checkGhostCaches(const Key& key){
		if(lruPart_->checkGhost(key)){
			if(lfuPart_->decreaseCapacity()){
				lruPart_->increaseCapacity();
			}

			return true;
		}

		if(lfuPart_->checkGhost(key)){
			if(lruPart_->decreaseCapacity()){
				lfuPart_->increaseCapacity();
			}

			return true;
		}

		return false;
	}

private:
	std::size_t capacity_;
	std::size_t transformThreshold_;
	std::mutex mutex_;
	std::unique_ptr<ArcLruPart<Key,Value>> lruPart_;
	std::unique_ptr<ArcLfuPart<Key,Value>> lfuPart_;
};

}