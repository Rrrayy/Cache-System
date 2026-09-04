# Cache-System

[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()

基于 C++17 模板实现的缓存策略库，提供统一接口、七种缓存策略及可复现的对比测试。

## 缓存策略

| 策略 | 核心机制 | 适用场景 |
|---|---|---|
| FIFO | 按写入顺序淘汰 | 简单缓存、性能基线 |
| LRU | 哈希表 + 双向链表 | 时间局部性明显 |
| LFU | 按访问频次分组 | 热点稳定、访问倾斜 |
| LFU-Aging | LFU + 频次衰减 | 热点会随时间变化 |
| LRU-K | 历史准入 + 主 LRU | 过滤一次性访问 |
| Hash-LRU | 多个独立 LRU 分片 | 高并发、降低锁竞争 |
| ARC | LRU/LFU 分区 + 幽灵反馈 | 访问模式不固定 |

## 设计概览

所有策略基于统一模板接口：

```cpp
template<typename Key,typename Value>
class cache_system{
public:
	virtual ~cache_system()=default;

	virtual void put(
		const Key& key,
		const Value& value
	)=0;

	virtual bool get(
		const Key& key,
		Value& value
	)=0;

	virtual Value get(
		const Key& key
	)=0;
};
```

推荐使用返回 `bool` 的查询接口，以区分缓存未命中和缓存值本身为默认值的情况。

```cpp
#include"RLruCache.hpp"

RrCache::RLruCache<std::string,int> cache(100);

cache.put("answer",42);

int value=0;
if(cache.get("answer",value)){
	std::cout<<value<<"\n";
}
```

## 核心实现

### LRU

使用哈希表平均 `O(1)` 定位节点，双向链表维护访问顺序。命中后将节点移动到最近使用端，容量满时淘汰最久未使用节点。

### LFU 与 Aging

节点按照访问频次放入不同链表，并维护当前最低频次。淘汰时优先删除频次最低的节点，同频次下淘汰最早进入该频次链表的节点。

当平均访问频次超过阈值时触发频次衰减，降低历史热点长期占据缓存的影响。衰减过程需要遍历所有节点，复杂度为 `O(N)`。

### LRU-K

新数据先进入历史区，达到指定访问次数后再进入主 LRU，用于过滤扫描和一次性访问造成的缓存污染。

### Hash-LRU

根据 key 的哈希值将请求路由到独立 LRU 分片：

```text
key
→ hash(key)
→ hash % shard_count
→ 对应LRU分片
```

不同分片拥有独立锁，可以降低多线程访问不同 key 时的锁竞争。代价是无法在分片之间共享空闲容量，命中率可能与全局 LRU 略有差异。

### ARC

本项目的 ARC 是基于幽灵反馈思想实现的 LRU/LFU 自适应混合策略：

```text
新数据进入LRU
→ 达到访问门槛后迁移到LFU
→ 淘汰的key进入对应幽灵缓存
→ 根据幽灵命中动态调整两个分区容量
```

幽灵缓存只记录 key，不保存 value。LRU 与 LFU 的真实容量之和受总容量约束，同一个 key 只存在于一个真实分区。

该实现并非论文 ARC 的严格复刻，详细设计见 [RArcCache/README.md](RArcCache/README.md)。

## 构建

环境要求：

- GCC 或 Clang
- C++17
- pthread

直接编译：

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread \
	Rtest.cpp -o test
```

运行：

```bash
./test
```

也可以使用 CMake：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## 测试结果

测试使用固定随机种子，所有策略执行相同的单线程请求序列。以下结果用于观察策略命中率，不代表生产环境性能。

| 策略 | 热点访问 | 循环扫描 | 负载切换 |
|---|---:|---:|---:|
| LRU | 49.62% | 4.54% | 54.89% |
| LFU | **66.71%** | **8.67%** | 38.40% |
| ARC | 59.92% | 4.54% | 54.85% |
| LRU-K | 54.72% | 8.41% | **56.87%** |
| LFU-Aging | **66.71%** | **8.67%** | 38.40% |
| FIFO | 37.31% | 4.59% | 53.93% |
| Hash-LRU | 48.78% | 4.54% | 55.10% |

结果表明：

- 稳定热点场景下，LFU 的命中率最高。
- 循环扫描场景下，LFU 和 LRU-K 的抗污染能力更好。
- 负载切换场景下，LRU-K 表现最好。
- Hash-LRU 的主要价值是降低多线程锁竞争，单线程测试无法体现其并发优势。
- 当前测试未观察到 Aging 对命中率的影响，需要使用热点迁移专项场景继续验证。

## 项目结构

```text
Cache-System/
├── cache_system.hpp
├── RFIFOCaChe.hpp
├── LruNode.hpp
├── RLruCache.hpp
├── RLfuCache.hpp
├── RLru-kCache.hpp
├── RHashLruCache.hpp
├── RArcCache/
│   ├── RArcCache.hpp
│   ├── RArcCacheNode.hpp
│   ├── RArcLruPart.hpp
│   ├── RArcLfuPart.hpp
│   └── README.md
├── Rtest.cpp
└── CMakeLists.txt
```

## 工程边界

当前项目主要用于缓存算法学习、实现对比和并发设计实验，尚未包含：

- TTL 与过期清理
- 容量按字节计算
- 持久化与故障恢复
- 命中率等运行时指标
- 严格的多线程性能基准
- 生产级异常与内存分配策略

## License

MIT
