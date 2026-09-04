#pragma once

#include"RLruCache.hpp"
#include"cache_system.hpp"

#include<algorithm>
#include<cstddef>
#include<functional>
#include<memory>
#include<thread>
#include<vector>

namespace RrCache{

template<typename Key,typename Value>
class RHashLruCache:public cache_system<Key,Value>{
private:
	std::size_t capacity_;
	std::size_t sliceNum_;
	std::vector<std::unique_ptr<RLruCache<Key,Value>>> lruSliceCaches_;

private:
	static std::size_t resolveSliceNum(
		std::size_t capacity,
		int sliceNum){
		std::size_t hardwareCount=
			static_cast<std::size_t>(
				std::thread::hardware_concurrency()
			);

		std::size_t sliceCount=sliceNum>0
			?static_cast<std::size_t>(sliceNum)
			:hardwareCount;

		if(sliceCount==0){
			sliceCount=1;
		}

		if(capacity>0){
			sliceCount=std::min(sliceCount,capacity);
		}

		return sliceCount;
	}

	std::size_t Hash(const Key& key) const{
		return std::hash<Key>{}(key);
	}

	std::size_t getSliceIndex(const Key& key) const{
		return Hash(key)%sliceNum_;
	}

public:
	explicit RHashLruCache(
		std::size_t capacity,
		int sliceNum=0)
		:capacity_(capacity),
		sliceNum_(resolveSliceNum(capacity,sliceNum)){
		lruSliceCaches_.reserve(sliceNum_);

		std::size_t baseSize=capacity_/sliceNum_;
		std::size_t remainder=capacity_%sliceNum_;

		for(std::size_t index=0;index<sliceNum_;++index){
			std::size_t sliceSize=
				baseSize+(index<remainder?1:0);

			lruSliceCaches_.emplace_back(
				std::make_unique<RLruCache<Key,Value>>(
					static_cast<int>(sliceSize)
				)
			);
		}
	}

	~RHashLruCache() override=default;

	void put(
		const Key& key,
		const Value& value) override{
		std::size_t sliceIndex=getSliceIndex(key);
		lruSliceCaches_[sliceIndex]->put(key,value);
	}

	bool get(
		const Key& key,
		Value& value) override{
		std::size_t sliceIndex=getSliceIndex(key);
		return lruSliceCaches_[sliceIndex]->get(key,value);
	}

	Value get(const Key& key) override{
		Value value{};
		get(key,value);
		return value;
	}
};

}