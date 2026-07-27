# Cache-System

[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()

一个基于 C++17 模板的缓存策略库，提供六种缓存替换算法的完整实现。纯头文件、统一接口、内置线程安全。

---

## 目录

- [背景与动机](#背景与动机)
- [与已有方案的对比](#与已有方案的对比)
- [支持策略总览](#支持策略总览)
- [快速开始](#快速开始)
- [API 参考](#api-参考)
- [策略选型指南](#策略选型指南)
- [架构设计](#架构设计)
- [算法详解](#算法详解)
- [基准测试](#基准测试)
- [构建与测试](#构建与测试)
- [项目结构](#项目结构)
- [参考资料](#参考资料)
- [License](#license)

---

## 背景与动机

缓存替换策略是计算机系统中的一个经典问题。从 CPU 的 TLB 替换到数据库的 Buffer Pool，从 CDN 的内容缓存到 Redis 的 key 淘汰，不同场景对缓存策略有不同的需求。

业界已有的方案分为两类：

- **内置在特定系统中**（Redis 的 volatile-lru/allkeys-lfu、MySQL 的 LRU 变体），与系统耦合，无法独立复用。
- **通用缓存库**（Google Guava Cache、Caffeine），但主要是 Java 生态。

本项目的设计目标：

1. 覆盖从简单（FIFO）到复杂（ARC）的全频谱策略，方便对比和选型。
2. 纯头文件 + 模板泛型，零运行时依赖，嵌入现有项目成本低。
3. 内置频次衰减（LFU-Aging）和幽灵队列（ARC）等工业级优化，而非教科书简化版本。
4. 提供统一的 benchmark 框架，在同一访问序列下对比各策略的真实表现。



## 支持策略总览

| 策略 | 核心数据结构 | 时间复杂度 | 适用场景 |
|------|-------------|-----------|----------|
| FIFO | 队列 + 哈希表 | O(1) | 基线对比，或对淘汰策略不敏感的场景 |
| LRU | 双向链表 + 哈希表 | O(1) | 通用场景，时间局部性强 |
| LRU-K | 两级 LRU（历史 + 主缓存） | O(1) | 需要过滤一次性访问的冷数据 |
| Hash-LRU | N 个独立 LRU 分片 | O(1) 单分片 | 高并发读场景，降低锁竞争 |
| LFU | 按频次组织的双向链表 | O(1) | 访问频次极度倾斜，热点稳定 |
| ARC | LRU + LFU 双区 + 幽灵队列 | O(1) | 负载模式未知或经常变化 |

---

## 快速开始

```bash
git clone https://github.com/Rrrayy/Cache-System.git
cd Cache-System
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
./bin/test_cache
```

或直接编译（无需 CMake）：

```bash
g++ -std=c++17 Rtest.cpp -o test_cache -I. -I./RArcCache -O2
./test_cache
```

---

## API 参考

### 基类接口

```cpp
template<typename Key, typename Value>
class cache_system {
public:
    virtual ~cache_system() = default;
    virtual void put(Key key, Value value) = 0;          // 插入或更新
    virtual bool get(Key key, Value& value) = 0;         // 读取，未命中返回 false
    virtual Value get(Key key) = 0;                      // 读取，未命中返回默认值
};
```

### 各策略特有接口

| 类 | 额外接口 | 说明 |
|----|---------|------|
| RLruCache | `remove(Key key)` | 删除指定键 |
| RLfuCache | `purge()` | 清空所有缓存数据 |
| RLfuCache (构造参数) | `RLfuCache(capacity, maxAverageNum)` | 第二个参数控制频次衰减触发阈值 |
| RLruKCache (构造参数) | `RLruKCache(cap, historyCap, k)` | k 为进入主缓存所需的最小访问次数 |
| RHashLruCache (构造参数) | `RHashLruCache(capacity, sliceNum)` | sliceNum 默认取 CPU 核心数 |
| RArcCache (构造参数) | `RArcCache(capacity, transformThreshold)` | 第二个参数控制 LRU 晋升 LFU 的阈值 |

### 使用示例

```cpp
#include "RLruCache.hpp"
#include "RLfuCache.hpp"
#include "RArcCache/RArcCache.hpp"

RrCache::RLruCache<std::string, int> lru(100);
lru.put("key1", 42);
lru.remove("key1");                                  // 显式删除

RrCache::RLfuCache<int, std::string> lfu(100, 20000); // 频次衰减阈值 20000
lfu.purge();                                           // 清空

RrCache::RLruKCache<int, std::string> lruk(100, 1000, 2); // K=2
RrCache::RArcCache<int, std::string> arc(200);             // 推荐默认
RrCache::RFIFOCache<int, std::string> fifo(100);
```

---

## 策略选型指南

```
负载模式已知？
  是
  ├── 访问频率高度集中（20% 的 key 产生 80% 的访问）
  │   └── LFU / LFU-Aging（带频次衰减，防止热点僵化）
  ├── 时间局部性强（刚访问过很可能再访问）
  │   └── LRU
  ├── 存在大量一次性访问（扫库、爬虫场景）
  │   └── LRU-K（K >= 2，过滤冷数据）
  └── 顺序扫描居多
      └── ARC（所有策略在此场景都有限，ARC 相对最好）

负载模式不确定或经常变化？
  └── ARC

并发读极高？
  └── Hash-LRU（分片数 = CPU 核心数）

只需一个最简单的缓存做 baseline？
  └── FIFO
```

---

## 架构设计

### 类继承结构

```
cache_system<Key, Value>        （抽象接口）
  |
  +-- RFIFOCache<Key, Value>     （FIFO，不感知访问模式）
  +-- RLruCache<Key, Value>      （LRU：双向链表 + 哈希表）
  |     +-- RLruKCache<Key, Value>  （LRU-K：继承 LRU，增加历史准入）
  +-- RLfuCache<Key, Value>      （LFU：按频次索引的链表）
  +-- RHashLruCache<Key, Value>  （分片 LRU：封装 N 个独立 RLruCache）
  +-- RArcCache<Key, Value>      （ARC：组合 ArcLruPart + ArcLfuPart）
```

### 节点模型

每个缓存节点使用 `std::shared_ptr` 管理生命周期。前驱指针使用 `std::weak_ptr`，打断双向链表中 shared_ptr 之间的循环引用。

```
+------------------+     +------------------+     +------------------+
|  哨兵头节点       | --> |  数据节点(k,v)   | --> |  数据节点(k,v)   | --> 哨兵尾节点
| (dummyHead)      | <-- | prev_(weak_ptr)  | <-- | prev_(weak_ptr)  |
+------------------+     +------------------+     +------------------+
```

哨兵节点消除了链表操作中的所有空指针判断，减少热点路径上的分支。

---

## 算法详解

### FIFO（RFIFOCache）

使用 `std::queue<Key>` 记录插入顺序。缓存满时淘汰队头元素。不感知访问频率和近期度，适合作为性能基线。

### LRU（RLruCache）

双向链表按最近访问时间排序。命中时，节点移至链表尾部（最近使用端）。淘汰时，移除链表头部节点（最近最少使用端）。`unordered_map` 提供 key 到节点的 O(1) 索引。

### LRU-K（RLruKCache）

继承 RLruCache，增加第二个 LRU 实例作为历史记录缓存。键在前 K-1 次访问时只记录频次，不缓存值。达到 K 次后，将数据从历史缓存晋升到主缓存。`historyValueMap_` 暂存尚未晋升的数据值，确保首次晋升时不丢失。

K=2 时效果最实用：一次访问不缓存，第二次才进入主缓存，过滤扫库、爬虫等一次性访问。

### LFU 带频次衰减（RLfuCache）

按频次组织数据，每个频次对应一个双向链表（`FreqList`）。`freqToFreqList_` 提供频次到链表的 O(1) 映射，`minFreq_` 跟踪当前最低非空频次。

命中时，节点从当前频次链表移除，追加到 freq+1 链表。若原链表变空且该频次是 minFreq_，则递增。

**频次衰减**：当缓存平均访问频次超过阈值时，将所有节点的频次减半（最低为 1）。解决朴素 LFU 中早期热点长期占据缓存、无法响应负载变化的问题。阈值通过构造函数参数 `maxAverageNum` 配置，默认 1,000,000。

### 分片 LRU（RHashLruCache）

封装 N 个独立 RLruCache，每个管理 `ceil(capacity / N)` 个槽位。key 所属分片由 `hash(key) % N` 确定。分片数默认取 `std::thread::hardware_concurrency()`。

分布均匀时，锁竞争降低到原来的 1/N。各分片独立向上取整，总有效容量可能比指定容量多 N-1 个槽位。

### ARC（RArcCache）

ARC 将缓存划分为四个区域：

| 区域 | 内容 | 作用 |
|------|------|------|
| LRU 主区 | 近期访问的数据和值 | 保留时间局部性高的数据 |
| LFU 主区 | 高频访问的数据和值 | 保留频次高的数据 |
| LRU 幽灵区 | 从 LRU 主区淘汰的 key（仅元数据） | 反馈 LRU 侧容量是否不足 |
| LFU 幽灵区 | 从 LFU 主区淘汰的 key（仅元数据） | 反馈 LFU 侧容量是否不足 |

总容量在 LRU 和 LFU 之间动态划分。幽灵命中即触发容量调整：

- LRU 幽灵命中 -> LRU 容量 +1，LFU 容量 -1
- LFU 幽灵命中 -> LFU 容量 +1，LRU 容量 -1

`transformThreshold_` 控制 LRU 侧数据被访问多少次后晋升到 LFU 侧，防止低频数据占用 LFU 容量。

参考论文：Megiddo & Modha, "ARC: A Self-Tuning, Low Overhead Replacement Cache" (FAST '03)。

---

## 基准测试

所有测试在单线程、相同访问序列下运行，各策略使用独立缓存实例，对比命中率。

运行截图：

<img width="984" alt="benchmark" src="https://github.com/user-attachments/assets/806c7aca-ded5-4a9c-8847-baa8990715ba" />

### 测试一：热点数据访问

容量 20，总操作 500,000 次，70% 集中在 20 个热点键，30% 随机落在 5,000 个冷键。

| 策略 | 命中率 |
|------|--------|
| LRU | 49.52% |
| LFU | 66.76% |
| ARC | 66.05% |
| LRU-K | 53.69% |
| LFU-Aging | **66.81%** |
| FIFO | 37.30% |

基于频次的策略（LFU、ARC）显著优于 LRU 和 FIFO。工作集能完全装入缓存时，频次预测能力优于近期度。

### 测试二：循环扫描

容量 50，总操作 200,000 次，60% 顺序遍历 500 个键，30% 随机跳跃，10% 范围外访问。

| 策略 | 命中率 |
|------|--------|
| LRU | 4.50% |
| LFU | 8.92% |
| ARC | **9.54%** |
| LRU-K | 7.72% |
| LFU-Aging | 8.89% |
| FIFO | 4.69% |

顺序扫描下所有策略退化到接近冷启动水平。ARC 因幽灵队列的自适应能力略占优势。

### 测试三：工作负载剧烈变化

容量 30，总操作 80,000 次，分为 5 个阶段（热点 -> 大范围随机 -> 顺序扫描 -> 局部随机 -> 混合）。

| 策略 | 命中率 |
|------|--------|
| LRU | 55.02% |
| LFU | 36.57% |
| ARC | **59.26%** |
| LRU-K | 56.47% |
| LFU-Aging | 37.05% |
| FIFO | 53.83% |

ARC 综合表现最优。纯 LFU 在此场景严重退化——早期阶段的频次累积污染了缓存状态，全局衰减来不及响应快速变化。

### 结论

| 场景 | 最优策略 | 要点 |
|------|---------|------|
| 热点集中访问 | LFU / LFU-Aging | 工作集可放入缓存时，频次优于近期度 |
| 循环扫描 | ARC | 所有策略都低，ARC 相对最好 |
| 负载突变 | ARC | LRU/LFU 自适应平衡的价值在此体现 |

对于负载模式不确定的生产环境，**ARC 是推荐的默认策略**。

---

## 构建与测试

### 环境要求

- C++17 或更高版本
- CMake 3.10+（可选）

### 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

调试模式（启用 AddressSanitizer + UndefinedBehaviorSanitizer）：

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
```

### 直接编译

```bash
g++ -std=c++17 Rtest.cpp -o test_cache -I. -I./RArcCache -O2
```

---

## 项目结构

```
Cache-System/
  cache_system.hpp           抽象接口基类
  LruNode.hpp                链表节点（shared_ptr/weak_ptr）
  RLruCache.hpp              LRU 实现
  RLfuCache.hpp              LFU + 频次衰减
  RLru-kCache.hpp            LRU-K + 历史缓存
  RHashLruCache.hpp          分片 LRU（高并发优化）
  RFIFOCaChe.hpp             FIFO 实现
  RArcCache/
    RArcCache.hpp            ARC 主控
    RArcCacheNode.hpp        ARC 节点
    RArcLruPart.hpp          ARC LRU 组件 + 幽灵队列
    RArcLfuPart.hpp          ARC LFU 组件 + 幽灵队列
    README.md                ARC 设计文档
  Rtest.cpp                  基准测试程序
  CMakeLists.txt             构建配置
```

---

## License

MIT
