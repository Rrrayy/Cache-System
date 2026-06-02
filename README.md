# 🔄 CacheSystem - 高性能缓存系统库

一个用 C++17 实现的高性能缓存系统库，包含多种经典缓存替换策略，支持线程安全，易于扩展。

---

## 📊 测试结果

### 🧪 运行截图
<img width="984" height="873" alt="2F0EE41022252F804600C1C4A32C92DF" src="https://github.com/user-attachments/assets/806c7aca-ded5-4a9c-8847-baa8990715ba" />


### 📈 结果分析

| 场景 | LRU | LFU | ARC | LRU-K | LFU-Aging | FIFO |
|------|-----|-----|-----|-------|-----------|------|
| **热点数据访问** | 49.52% | **66.76%** | 66.05% | 53.69% | **66.81%** | 37.30% |
| **循环扫描** | 4.50% | 8.92% | **9.54%** | 7.72% | 8.89% | 4.69% |
| **工作负载变化** | 55.02% | 36.57% | **59.26%** | 56.47% | 37.05% | 53.83% |

### 🏆 算法表现总结

| 算法 | 最佳场景 | 特点 |
|------|----------|------|
| **ARC** | 循环扫描、工作负载变化 | **综合性能最优**，自适应调整 |
| **LFU / LFU-Aging** | 热点数据访问 | 擅长识别热点数据 |
| **LRU-K** | 中等表现 | 需要访问 K 次才进入缓存 |
| **LRU** | 通用场景 | 实现简单，时间局部性 |
| **FIFO** | 简单场景 | 实现最简单，不考虑访问模式 |

---

## 📚 目录

- [功能特性](#功能特性)
- [支持的缓存策略](#支持的缓存策略)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [API 使用示例](#api-使用示例)
- [测试说明](#测试说明)
- [ARC 缓存详解](#arc-缓存详解)


---

## ✨ 功能特性

- 🚀 **多种缓存策略**：LRU、LFU、LRU-K、ARC、FIFO、Hash-LRU
- 🔒 **线程安全**：所有缓存实现均支持并发访问
- 📦 **头文件驱动**：纯头文件库，无需编译链接
- 🔧 **易于扩展**：基于模板设计，支持任意 Key/Value 类型
- 📊 **高性能**：O(1) 时间复杂度的读写操作
- 🌐 **跨平台**：支持 Linux、macOS、Windows

---

## 🎯 支持的缓存策略

| 策略 | 全称 | 适用场景 | 特点 | 命中率（综合） |
|------|------|----------|------|----------------|
| **FIFO** | First In First Out | 简单场景 | 先进先出，实现最简单 | ⭐⭐ |
| **LRU** | Least Recently Used | 通用场景 | 最近最少使用，实现简单 | ⭐⭐⭐ |
| **LRU-K** | LRU-K | 区分冷热数据 | 需要访问 K 次才进入缓存 | ⭐⭐⭐ |
| **Hash-LRU** | Hash Partitioned LRU | 高并发场景 | 分片设计，降低锁竞争 | ⭐⭐⭐⭐ |
| **LFU** | Least Frequently Used | 热点数据 | 最不常使用，适合访问频率分布不均匀 | ⭐⭐⭐⭐ |
| **ARC** | Adaptive Replacement Cache | 自适应场景 | 自动平衡 LRU/LFU，综合性能最优 | ⭐⭐⭐⭐⭐ |

---

## 📁 项目结构

```
CacheSystem/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 项目说明文档
├── cache_system.hpp        # 缓存系统基类接口
├── LruNode.hpp             # LRU 节点定义
├── RLruCache.hpp           # LRU 缓存实现
├── RLfuCache.hpp           # LFU 缓存实现
├── RLru-kCache.hpp         # LRU-K 缓存实现
├── RHashLruCache.hpp       # Hash-LRU 缓存实现
├── RFIFOCaChe.hpp          # FIFO 缓存实现
├── RArcCache/              # ARC 缓存模块
│   ├── README.md           # ARC 详细说明
│   ├── RArcCache.hpp       # ARC 主类
│   ├── RArcCacheNode.hpp   # ARC 节点定义
│   ├── RArcLfuPart.hpp     # ARC-LFU 部分
│   └── RArcLruPart.hpp     # ARC-LRU 部分 
└── Rtest.md                # 测试程序详细说明
```

---

## 🚀 快速开始

### 环境要求

- C++17 或更高版本
- CMake 3.10+ (可选，用于编译测试)

### 编译测试

```bash
# 克隆项目
git clone <your-repo-url>
cd CacheSystem

# 创建构建目录
mkdir -p build && cd build

# 配置并编译
cmake ..
make -j4

# 运行测试
./bin/test_cache
```

### 直接编译（无需 CMake）

```bash
g++ -std=c++17 Rtest.cpp -o test_cache -I. -IRArcCache -Wno-sign-compare
./test_cache
```

---

## 📖 API 使用示例

### 基本用法

```cpp
#include "RLruCache.hpp"
#include "RLfuCache.hpp"
#include "RArcCache/RArcCache.hpp"
#include "RFIFOCaChe.hpp"

int main() {
    // 创建 LRU 缓存，容量为 100
    RrCache::RLruCache<int, std::string> lruCache(100);
    
    // 添加数据
    lruCache.put(1, "value1");
    lruCache.put(2, "value2");
    
    // 获取数据
    std::string value;
    if (lruCache.get(1, value)) {
        std::cout << "Get key 1: " << value << std::endl;
    }
    
    // 创建 FIFO 缓存
    RrCache::RFIFOCache<int, std::string> fifoCache(50);
    fifoCache.put(10, "fifo_value");
    
    // 创建 ARC 缓存（推荐用于大多数场景）
    RrCache::RArcCache<std::string, int> arcCache(200);
    arcCache.put("key1", 42);
    
    return 0;
}
```

### 缓存基类接口

```cpp
template<typename Key, typename Value>
class cache_system {
public:
    virtual ~cache_system() = default;
    
    // 添加或更新缓存
    virtual void put(Key key, Value value) = 0;
    
    // 获取缓存（返回是否成功）
    virtual bool get(Key key, Value& value) = 0;
    
    // 获取缓存（返回值，不存在返回默认值）
    virtual Value get(Key key) = 0;
};
```

---

## 🧪 测试说明

### Rtest.cpp 测试程序

`Rtest.cpp` 包含三个测试场景：

**1. 热点数据访问测试**
- 缓存容量：20
- 总操作：500,000 次
- 热点数据：20 个（70% 访问概率）
- 冷数据：5,000 个（30% 访问概率）

**2. 循环扫描测试**
- 缓存容量：50
- 总操作：200,000 次
- 循环范围：500 个键

**3. 工作负载剧烈变化测试**
- 缓存容量：30
- 总操作：80,000 次
- 5 个阶段，每个阶段有不同的访问模式



## 📝 ARC 缓存详解

ARC (Adaptive Replacement Cache) 是一种自适应缓存替换策略，自动平衡 LRU 和 LFU 的优点。

### 核心设计

ARC 维护两个部分：
- **LRU 部分**：存储最近访问的数据
- **LFU 部分**：存储访问频繁的数据

### 自适应机制

ARC 通过幽灵缓存（Ghost Cache）追踪被淘汰的数据：
1. 如果 LRU 幽灵缓存命中 → 增加 LRU 容量，减少 LFU 容量
2. 如果 LFU 幽灵缓存命中 → 增加 LFU 容量，减少 LRU 容量

### 查看详细文档

请查看 `RArcCache/README.md` 获取 ARC 的完整实现说明。

---



