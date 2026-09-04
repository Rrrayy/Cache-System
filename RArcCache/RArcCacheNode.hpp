#pragma once

#include<cstddef>
#include<memory>

namespace RrCache{

template<typename Key,typename Value>
class ArcNode{
private:
	Key key_;
	Value value_;
	std::size_t accessCount_;
	std::weak_ptr<ArcNode> prev_;
	std::shared_ptr<ArcNode> next_;

public:
	ArcNode()
		:accessCount_(1),
		next_(nullptr){}

	ArcNode(const Key& key,const Value& value)
		:key_(key),
		value_(value),
		accessCount_(1),
		next_(nullptr){}

	const Key& getKey() const{
		return key_;
	}

	const Value& getValue() const{
		return value_;
	}

	std::size_t getAccessCount() const{
		return accessCount_;
	}

	void setValue(const Value& value){
		value_=value;
	}

	void incrementAccessCount(){
		++accessCount_;
	}

	template<typename K,typename V>
	friend class ArcLruPart;

	template<typename K,typename V>
	friend class ArcLfuPart;
};

}