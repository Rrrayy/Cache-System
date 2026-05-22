#pragma once
namespace RrCache{
    template<typename Key,typename Value>
    class cache_system{
        public:
            virtual ~cache_system() {};
            //添加缓存接口
            virtual void put(Key key,Value value)=0;
            //传入key，判断是否存在对应value，若存在则用参数得到value值
            virtual bool get(Key key,Value &value)=0;
            //若找到key，则直接返回value
            virtual Value get(Key key)=0;

    };
}