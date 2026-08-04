# RArcCache —— 自适应 LRU/LFU 混合缓存（ARC 思想实现）

> 受 ARC（Adaptive Replacement Cache, Megiddo & Modha 2003）启发的缓存淘汰策略：用**幽灵缓存（Ghost Cache）**反馈自适应调整 LRU 与 LFU 两个分区的容量比例，在时间局部性与频率局部性之间自动平衡。
>
> ⚠️ **定位声明**：本实现不是论文 ARC 的精确复刻，而是保留其"幽灵反馈"核心思想的混合策略（差异见[与原版 ARC 的差异](#与原版-arc-的差异)）。

## 1. 设计动机

缓存淘汰策略解决的根本问题：**容量有限，如何决定谁被驱逐，使未来命中率最高**。

| 策略 | 擅长 | 短板 |
|------|------|------|
| LRU | 时间局部性（近期访问短期复用） | 循环扫描（sequential flooding）下被反复击穿 |
| LFU | 频率局部性（长期热点稳定驻留） | 对访问模式变化响应慢；历史热点长期占坑（pollution） |
| ARC 思想 | 两者自适应，不预先假设负载类型 | 实现复杂度高 |

两个典型失败模式：
- **LRU 被循环扫描击穿**：容量 10、顺序扫描 100 个数据，每次访问都淘汰即将被再次访问的数据，命中率趋近 0
- **LFU 被历史热点污染**：某 key 曾访问 10⁶ 次后不再访问，频率计数仍让它长期占坑

ARC 思想的答案：**不猜负载，用被驱逐数据的"幽灵"检测访问模式，动态转移分区容量**。

## 2. 架构总览

```
                    ┌───────────────────────────────────────┐
                    │          RArcCache（无全局锁）          │
                    └───────────────────────────────────────┘
        put/get(key)                │
              │                     ▼
              │      ┌──────────────────────────────┐
              │      │   checkGhostCaches(key)       │
              │      └──────────────────────────────┘
              │          │                  │
              │   LRU 幽灵命中?        LFU 幽灵命中?
              │        │                  │
              │   LFU 容量-1, LRU+1   LRU 容量-1, LFU+1
              ▼        ▼                  ▼
      ┌──────────────────────┐   ┌──────────────────────┐
      │  ArcLruPart（容量自调） │   │  ArcLfuPart（容量自调） │
      │  unordered_map         │   │  unordered_map        │
      │  + 双向链表（哨兵）      │   │  + freqMap_+minFreq_  │
      │  put/get/evict O(1)    │   │  get 最坏 O(c)        │
      │  (各自一把 mutex)       │   │  (各自一把 mutex)      │
      │                        │   │                       │
      │  计数 ≥ 阈值 ──复制──▶ │   │  （LRU 副本保留）       │
      └──────────┬───────────┘   └──────────┬───────────┘
                 │ 驱逐（节点复用）           │ 驱逐（节点复用）
                 ▼                          ▼
      ┌──────────────────────┐   ┌──────────────────────┐
      │  LRU 幽灵（FIFO）      │   │  LFU 幽灵（FIFO）      │
      │  存完整节点（含 value） │   │  存完整节点（含 value） │
      │  ghostCapacity=c      │   │  ghostCapacity=c      │
      └──────────────────────┘   └──────────────────────┘
```

**实现要点（对应源码）**：
- **双哨兵链表**：主链表与幽灵链表各用 `mainHead_/mainTail_`、`ghostHead_/ghostTail_` 哨兵，空表判断与删除逻辑统一，免去 nullptr 特判
- **节点复用**：被驱逐节点**不销毁**，直接改链表指针挂入幽灵缓存（LRU 幽灵入口处 `accessCount_` 重置为 1）——零分配驱逐，但也意味着幽灵缓存保留着 value
- **复制式晋升**：key 在 LRU 命中且计数达标 → `lfuPart_->put(key, value)` 在 LFU 插入副本，**LRU 内副本不清除**，之后该 key 的访问仍走 LRU 并继续累加计数

## 3. 核心机制

### 3.1 幽灵反馈环（容量自适应）

直觉：**幽灵命中 = 该分区"历史容量不足"的直接证据**。

- key 被 LRU 驱逐后再次被访问（LRU 幽灵命中）→ 说明 LRU 当初太小装不下它 → **LFU 容量 -1、LRU 容量 +1**
- 反之 LFU 幽灵命中 → **LRU 容量 -1、LFU 容量 +1**

容量转移是**逐次微调（±1）**，随请求流持续形成负反馈：分区频繁"误杀"数据 → 幽灵频繁命中 → 容量流向该分区 → 误杀率下降 → 动态平衡。两个分区各持初始容量 c，反馈收敛到适应负载的稳定比例（总和 ≈ 2c 量级）。

```cpp
// RArcCache.hpp 核心：每次访问先做幽灵检查
bool checkGhostCaches(Key key) {
    if (lruPart_->checkGhost(key)) {          // LRU 幽灵命中：LRU 容量不足的证据
        if (lfuPart_->decreaseCapacity()) {   // LFU 若已满先驱逐，再让出容量
            lruPart_->increaseCapacity();
        }
        return true;
    }
    else if (lfuPart_->checkGhost(key)) {     // LFU 幽灵命中：反之
        if (lruPart_->decreaseCapacity()) {
            lfuPart_->increaseCapacity();
        }
        return true;
    }
    return false;
}
```

注意：`increaseCapacity()` 无上限；`decreaseCapacity()` 先驱逐后减量（`size == capacity_` 时触发），保证容量下调不超载。

### 3.2 LRU → LFU 晋升（transformThreshold）

```cpp
// ArcLruPart::updateNodeAccess
bool updateNodeAccess(NodePtr node) {
    moveToFront(node);
    node->incrementAccessCount();
    return node->getAccessCount() >= transformThreshold_;
}
// get() 命中且返回 true 时，主类在 LFU 复制一份
```

- `transformThreshold` 越小越偏向 LFU 行为（频率主导），越大越偏向 LRU 行为（recency 主导）
- **只有 get 计数**：`put()` 只更新值并移到表头，不涨访问计数
- 与 Segmented LRU（probation/protected 两段）思想同源，但用"计数达标"代替"访问即晋升"，多一个可调维度

### 3.3 驱逐路径

| 分区 | 驱逐对象 | 实现 |
|------|----------|------|
| LRU | 主链表尾部（最久未访问） | O(1)：`mainTail_->prev_` 摘除 |
| LFU | `freqMap_[minFreq_]` 队头 | O(log F)：红黑树找 `begin()` 更新 `minFreq_` |
| 幽灵 | FIFO 淘汰最旧 | O(1)（LRU 头插尾删 / LFU 尾插头删） |

驱逐的节点统一进幽灵：LRU 入口重置 `accessCount_ = 1`；LFU 入口**不重置**（该频率不再使用，仅留作历史）。

### 3.4 复杂度总表（如实标注）

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| LRU put / get / evict | O(1) 均摊 | unordered_map + 双向链表 |
| LFU put | O(1) 均摊 | 插入即 `freqMap_[1]` 队尾 |
| LFU get | **O(桶大小)，最坏 O(c)** | 频率更新用 `std::list::remove` 线性扫描（`ArcLfuPart::updateNodeFrequency`） |
| 容量调整 / 幽灵检查 | O(1) | 仅整数增减 + 哈希查询 |
| 空间 | O(c) 主缓存 + O(c) 幽灵 | 幽灵存**完整节点**（含 value），实际占用 2× 节点数 |

> 相比"教科书 O(1)"：LFU 的频率更新是线性 `remove`，命中集中在少数高频桶时退化明显。改造方案见[优化方向](#8-优化方向)。

## 4. 与原版 ARC 的差异

| 维度 | 论文 ARC | 本实现 |
|------|----------|--------|
| 分区结构 | T1（近期）+ T2（远期），**两条 LRU 链表** | LRU 分区 + LFU 分区，频率显式化 |
| 幽灵 | B1/B2，总量受控 | 双 FIFO 幽灵，各容量 = 初始 c |
| 自适应 | 幽灵命中按 δ 增量微调 p（T1 目标比例） | 每次 ±1 转移容量 |
| 容量约束 | 全局 c，T1+T2 = c，幽灵 ≤ c | **无全局约束**，两分区独立（总和 ≈ 2c） |
| 频率维度 | 无显式计数，靠层级近似 | `accessCount_` + 迁移阈值，可调 |
| 晋升行为 | 链表间**移动** | 复制（原分区副本保留） |

**为什么这么设计**：频率显式化让"热数据"判定可调参、语义清晰；代价是丢失论文 ARC 的严格内存界限（2c~4c）与更细的自适应粒度。若面试官追问"和真 ARC 的区别"，按此表回答即可。

## 5. API 文档

继承自 `cache_system<Key, Value>`（纯虚：`put` / `get(Key, Value&)` / `get(Key)`），重写实现：

```cpp
// 构造：capacity 为 LRU、LFU 各分区的初始容量；transformThreshold 为 LRU→LFU 晋升阈值（≥1）
explicit RArcCache(size_t capacity = 10, size_t transformThreshold = 2);

// 写入：key 存在则更新；分区满则驱逐本分区尾部
void put(Key key, Value value) override;

// 读取：命中返回 true 并填充 value；LRU 命中且计数达标 → 复制入 LFU
bool get(Key key, Value& value) override;

// 便捷版：命中返回 value，未命中返回 Value{} 默认值
Value get(Key key) override;
```

```cpp
// 使用示例
RrCache::RArcCache<int, std::string> cache(100, 2);  // 每分区容量 100，晋升阈值 2

cache.put(1, "hello");
cache.put(2, "world");

std::string v;
if (cache.get(1, v)) {
    std::cout << v << std::endl;  // hello
}
// key=1 被 get 访问 2 次后，自动复制进 LFU 分区
```

**⚠️ 线程安全（重要）**：锁在**分区内部**（`ArcLruPart::mutex_` / `ArcLfuPart::mutex_`），单分区操作线程安全；但主类的复合操作（幽灵检查 + 双分区更新）**非原子**，且 `ArcLfuPart::contain()` 未加锁。**并发写入同一 key 存在数据竞争**。当前定位为单线程使用，或由调用方加外部锁。

## 6. 工程细节与已知问题

### 6.1 节点内存布局

```cpp
template<typename Key, typename Value>
class ArcNode {
	Key key_;
	Value value_;
	size_t accessCount_;                     // LFU 频率依据（初始 1）
	std::weak_ptr<ArcNode> prev_;             // 仅导航，不持有所有权
	std::shared_ptr<ArcNode> next_;           // 唯一所有权链
};
```

- **所有权单向持有**：链表存活由 `next_` 的 shared_ptr 链保证，`prev_` 只做导航——避免双向 shared_ptr 循环引用
- **内存开销**：节点 ≈ 8(key) + 8(count) + 16(weak_ptr) + 16(shared_ptr) + 哈希桶 ≈ 50~60 B 结构开销；幽灵缓存存完整节点，**总节点数上界 ≈ 2× 主缓存 + 2× 幽灵 ≈ 4×capacity**，百万级容量需按 ~200 MB 估内存

### 6.2 ⚠️ shared_ptr 链式析构递归爆栈（真实风险）

```cpp
// 链表析构时：头哨兵 next_ 归零 → 递归析构下一节点 → …… 深度 = 链表长度
// 容量 10 万级节点 → 栈溢出（表现为 segfault，不是内存泄漏）
```

**解决方案（任选）**：
1. 析构前遍历断开：`while (head) { auto nxt = head->next_; head->next_.reset(); head = nxt; }`
2. 节点改用裸指针 `ArcNode*`，生命周期由缓存类统一管理（创建/驱逐显式 delete）——更贴近生产实现，顺带去掉引用计数原子开销

### 6.3 死参数与冗余

- `ArcLfuPart::transformThreshold_` 构造传入但**从未使用**——可删，或未来实现"LFU 冷门降级回 LRU"时启用
- LFU 幽灵入口未重置 `accessCount_`，与 LRU 幽灵行为不一致（当前无影响，重构时统一）

### 6.4 边界情况

| 场景 | 行为 |
|------|------|
| `capacity == 0` | `put` 直接返回 false，不缓存 |
| key 已在 LFU，LRU 又插一份 | 两分区各持一份；LRU 淘汰该副本后幽灵命中会触发容量转移 |
| `transformThreshold == 1` | 访问一次即复制入 LFU，近似 Segmented LRU |
| 幽灵满 | FIFO 淘汰最旧幽灵（`removeOldestGhost`） |

## 7. 性能参考

> 开发者本机自测数据（合成负载，`Rtest.cpp` 内置场景），非严格基准。完整场景与参数见仓库 `Rtest.cpp`。

**场景：热点 + 冷数据混合（容量 20，50 万次操作，20 热点 / 5000 冷 key）**

| 策略 | 命中率 | 解读 |
|------|--------|------|
| ARC（本实现） | 59.29% | 幽灵反馈把容量移向热点侧 |
| LRU | 54.90% | 冷数据偶发访问挤占热点 |
| LFU | 37.74% | 前段低频访问污染频率统计 |
| LRU-K | 54.54% | 抗污染但 K 固定，无自适应 |

**场景：循环扫描（容量 10，扫描集 100）**

| 策略 | 命中率 | 解读 |
|------|--------|------|
| ARC（本实现） | 9.65% | 扫描数据反复"误入"缓存 → 幽灵持续命中 → 容量向低频侧倾斜 |
| LFU | 8.85% | 天然不缓存单次访问 |
| LRU | 4.50% | 每次访问都淘汰即将复用的数据，最差 |

## 8. 优化方向

1. **LFU 频率更新 O(1) 化**：`freqMap_` 桶内改双向链表节点直连（LeetCode 460 方案），消除 `list::remove` 线性扫描
2. **线程安全**：主类加锁或改分片锁（sharded）→ 读路径双检 → 跨分区操作原子化；`contain()` 补锁
3. **节点去智能指针化**：裸指针 + 显式生命周期，消除引用计数原子开销与析构递归爆栈
4. **晋升改迁移**：达标后从 LRU 摘除再入 LFU，杜绝双分区同 key 并存（需处理"LRU 幽灵记录的 key 已在 LFU"的情况）
5. **接入论文 ARC 的 p 参数**（B1 命中 p+δ₁、B2 命中 p-δ₂），作为严格模式开关
