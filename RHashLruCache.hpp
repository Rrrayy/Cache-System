#pragma once
/**
@brief 分片式LRU缓存（线程安全优化）
针对多线程高并发场景下的缓存访问数据，采用hash分片策略

@description
1. 分片原理：
    -将总容量划分为多个独立的LRU缓存分片（默认分片数=cpu数量）
    -通过哈希函数将Key均匀散列在不同分片
    -每个分片独立管理自己的数据，互不影响
2. 并发优势：
    -多线程访问不同分片时，锁竞争概率大幅降低
    -理想情况下（Key均匀分布），并发性能接近线性提升
    -避免单个大锁导致的线程阻塞

3. 适用场景：
    -读多写少的高并发缓存访问
    -Key的分布较为均匀，避免热点Key集中在同一分片
    -对缓存一致性要求不极端严格的场景
  
  @tparam Key   缓存键类型，需要支持 std::hash
  @tparam Value 缓存值类型
 */
 #include"RLruCache.hpp"
 #include<vector>
 #include<memory>
 #include<functional>
 #include<thread>
 #include<cmath>
 #include<cstring>
 namespace RrCache{
    template<typename Key, typename Value>
    class RHashLruCache{
    private:
        size_t capacity_//总容量
        int sliceNum_;//切片数量
        std::vector<std::unique_ptr<RLruCache<Key,Value>>> lruSliceCaches_;//切片LRU缓存
    private:
        //将key转换成对应的hash值
        size_t Hash(Key,key){
            std::hash<key> hashFunc;
            retunr hashFunc(key);
        }
    public:
        RHashLruCache(size_t capacity,int sliceNum)
        :capacity(capacity_)
        ,sliceNum_(sliceNum>0?sliceNum:std::thread::hardware_concurrency()){
            size_t sliceSize=
        }
    };
 }