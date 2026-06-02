# 📚 ARC 缓存模块详解

## 概述

ARC (Adaptive Replacement Cache) 是一种自适应缓存替换策略，由 Nimrod Megiddo 和 Dharmendra S. Modha 在 2003 年提出。它自动平衡 LRU（最近最少使用）和 LFU（最不常使用）的优点，在各种工作负载下都能表现出色。

---

## 🎯 ARC 核心思想

### 为什么需要 ARC？

| 策略 | 优点 | 缺点 |
|------|------|------|
| **LRU** | 适应时间局部性，实现简单 | 对循环扫描表现差 |
| **LFU** | 适应频率局部性，热点数据稳定 | 对工作负载变化响应慢 |
| **ARC** | 自动平衡两者，综合性能最优 | 实现相对复杂 |

### ARC 的自适应机制

ARC 通过**幽灵缓存（Ghost Cache）**来学习访问模式：

1. **追踪淘汰历史**：被淘汰的数据不会立即消失，而是进入幽灵缓存
2. **检测重复访问**：如果幽灵缓存中的数据再次被访问，说明该数据值得缓存
3. **动态调整容量**：根据幽灵缓存的命中情况，调整 LRU 和 LFU 部分的容量比例

---

## 📁 文件结构

```
RArcCache/
├── README.md           # 本文件
├── RArcCache.hpp       # ARC 主类（对外接口）
├── RArcCacheNode.hpp   # 节点定义
├── RArcLruPart.hpp     # LRU 部分实现
└── RArcLfuPart.hpp     # LFU 部分实现
```

---

## 🔧 类设计详解

### 1. ArcNode - 节点类

```cpp
template<typename Key, typename Value>
class ArcNode {
private:
    Key key_;                    // 键
    Value value_;                // 值
    size_t accessCount_;         // 访问计数（用于 LFU）
    std::weak_ptr<ArcNode> prev_; // 前驱指针（弱引用，防止循环引用）
    std::shared_ptr<ArcNode> next_; // 后继指针
};
```

**关键点**：
- 使用 `weak_ptr` 作为前驱指针，避免智能指针循环引用导致内存泄漏
- `accessCount_` 记录访问次数，用于 LFU 部分的频率统计

### 2. ArcLruPart - LRU 部分

负责管理**最近访问**的数据，使用双向链表实现。

**核心方法**：

| 方法 | 功能 | 时间复杂度 |
|------|------|------------|
| `put()` | 添加/更新缓存 | O(1) |
| `get()` | 获取缓存 | O(1) |
| `evictLeastRecent()` | 驱逐最久未访问的节点 | O(1) |
| `checkGhost()` | 检查幽灵缓存 | O(1) |

**内部结构**：
- `mainCache_`: 主缓存（哈希表 + 双向链表）
- `ghostCache_`: 幽灵缓存（记录被淘汰的数据）

### 3. ArcLfuPart - LFU 部分

负责管理**访问频繁**的数据，使用频率链表实现。

**核心方法**：

| 方法 | 功能 | 时间复杂度 |
|------|------|------------|
| `put()` | 添加/更新缓存 | O(1) |
| `get()` | 获取缓存并更新频率 | O(1) |
| `evictLeastFrequent()` | 驱逐访问频率最低的节点 | O(1) |
| `checkGhost()` | 检查幽灵缓存 | O(1) |

**内部结构**：
- `mainCache_`: 主缓存（哈希表）
- `freqMap_`: 频率到节点列表的映射（map<int, list>）
- `minFreq_`: 当前最小频率

### 4. RArcCache - ARC 主类

整合 LRU 和 LFU 部分，实现自适应策略。

**核心逻辑**（`put()` 方法）：

```cpp
void put(Key key, Value value) {
    // 1. 检查幽灵缓存，调整容量比例
    checkGhostCaches(key);
    
    // 2. 更新 LRU 部分
    lruPart_->put(key, value);
    
    // 3. 如果 LFU 部分已有该键，同步更新
    if (lfuPart_->contain(key)) {
        lfuPart_->put(key, value);
    }
}
```

**自适应调整**（`checkGhostCaches()` 方法）：

```cpp
bool checkGhostCaches(Key key) {
    if (lruPart_->checkGhost(key)) {
        // LRU 幽灵命中 → 增加 LRU 容量
        if (lfuPart_->decreaseCapacity()) {
            lruPart_->increaseCapacity();
        }
        return true;
    } 
    else if (lfuPart_->checkGhost(key)) {
        // LFU 幽灵命中 → 增加 LFU 容量
        if (lruPart_->decreaseCapacity()) {
            lfuPart_->increaseCapacity();
        }
        return true;
    }
    return false;
}
```

---

## 🔄 工作流程图

```
用户请求
    │
    ▼
┌─────────────────────────────────────────────┐
│           checkGhostCaches(key)             │
├─────────────────────────────────────────────┤
│  LRU幽灵命中? ──Yes──▶ LRU容量+1, LFU容量-1 │
│       │                                     │
│      No                                     │
│       ▼                                     │
│  LFU幽灵命中? ──Yes──▶ LFU容量+1, LRU容量-1 │
│       │                                     │
│      No                                     │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│              数据访问操作                    │
├─────────────────────────────────────────────┤
│  get(key):                                  │
│    1. 先查 LRU 部分                         │
│    2. 如果访问次数达标，迁移到 LFU           │
│    3. 否则查 LFU 部分                       │
│                                             │
│  put(key, value):                           │
│    1. 更新 LRU 部分                         │
│    2. 如果 LFU 已有，同步更新               │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│              容量不足时                      │
├─────────────────────────────────────────────┤
│  LRU 满了 → 淘汰最久未访问 → 进入 LRU幽灵   │
│  LFU 满了 → 淘汰频率最低 → 进入 LFU幽灵     │
└─────────────────────────────────────────────┘
```

---

## 📊 与其他策略的对比

### 测试场景：工作负载剧烈变化

| 策略 | 命中率 | 特点 |
|------|--------|------|
| **ARC** | 59.29% | 自适应调整，表现稳定 |
| **LRU** | 54.90% | 对变化响应较快 |
| **LFU** | 37.74% | 对变化响应较慢 |
| **LRU-K** | 54.54% | 区分冷热，但不够灵活 |

### 测试场景：循环扫描

| 策略 | 命中率 | 特点 |
|------|--------|------|
| **ARC** | 9.65% | 自动切换到 LFU 模式 |
| **LFU** | 8.85% | 本身适合重复访问 |
| **LRU** | 4.50% | 循环扫描会不断淘汰 |

---

## 💡 使用建议

### 什么时候选择 ARC？

1. **工作负载不确定**：ARC 能自动适应各种模式
2. **混合访问模式**：既有热点数据又有循环扫描
3. **需要稳定性能**：ARC 在各种场景下表现都不错

### 什么时候不选择 ARC？

1. **简单场景**：如果访问模式很单一（比如纯 LRU 或纯 LFU），直接用对应策略更高效
2. **内存受限**：ARC 需要维护幽灵缓存，额外占用内存

---

## 📝 代码示例

```cpp
#include "RArcCache/RArcCache.hpp"

// 创建 ARC 缓存
// 参数1: 总容量
// 参数2: 转换阈值（LRU→LFU 的访问次数）
RrCache::RArcCache<int, std::string> cache(100, 2);

// 添加数据
cache.put(1, "hello");
cache.put(2, "world");

// 获取数据
std::string value;
if (cache.get(1, value)) {
    std::cout << value << std::endl;  // 输出: hello
}

// ARC 会自动学习访问模式
// 如果 key=1 被访问多次，会从 LRU 迁移到 LFU
```

---

## 🎯 关键设计要点

### 1. 线程安全

所有方法都使用 `std::lock_guard<std::mutex>` 保证线程安全：

```cpp
bool get(Key key, Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    // ... 线程安全的操作
}
```

### 2. 智能指针使用

- `shared_ptr`: 用于共享所有权（节点之间的链接）
- `weak_ptr`: 用于打破循环引用（前驱指针）

### 3. 幽灵缓存机制

幽灵缓存只存储键的引用，不存储值，因此内存开销很小，但能有效追踪访问模式。

---

