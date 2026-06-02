#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include <array>
#include "cache_system.hpp"
#include "RLfuCache.hpp"
#include "RLruCache.hpp"
#include "RLru-kCache.hpp"
#include "RArcCache/RArcCache.hpp"
#include "RFIFOCaChe.hpp"  // 引入 FIFO 缓存头文件

/**
 * @brief 计时器类，用于测量代码执行时间
 * 
 * 使用 std::chrono 库实现高精度计时，单位为毫秒
 */
class Timer {
public:
    /**
     * @brief 构造函数，记录开始时间
     */
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    /**
     * @brief 获取从构造到调用时经过的毫秒数
     * @return 经过的时间（毫秒）
     */
    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;  // 开始时间点
};

/**
 * @brief 打印测试结果
 * 
 * 计算并输出各算法的命中率，格式化为百分比
 * 
 * @param testName 测试名称
 * @param capacity 缓存容量
 * @param get_operations 各算法的 get 操作次数数组
 * @param hits 各算法的命中次数数组
 */
void printResults(const std::string& testName, int capacity,
                 const std::vector<int>& get_operations,
                 const std::vector<int>& hits) {
    std::cout << "=== " << testName << " ===" << std::endl;
    std::cout << "缓存容量：" << capacity << std::endl;

    // 根据算法数量选择对应的名称列表
    std::vector<std::string> names;
    if (hits.size() == 3) {
        names = {"LRU", "LFU", "ARC"};
    } else if (hits.size() == 4) {
        names = {"LRU", "LFU", "ARC", "LRU-K"};
    } else if (hits.size() == 5) {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    } else if (hits.size() == 6) {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging", "FIFO"};  // 添加 FIFO
    }

    // 遍历输出每个算法的命中率
    for (size_t i = 0; i < hits.size(); ++i) {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << (i < names.size() ? names[i] : "算法 " + std::to_string(i+1))
                  << " - 命中率: " << std::fixed << std::setprecision(2)
                  << hitRate << "% ";
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }

    std::cout << std::endl;
}

/**
 * @brief 测试场景1：热点数据访问测试
 * 
 * 模拟大部分访问集中在少数热点数据上，少量访问分散在冷数据上的场景
 * 测试各算法对热点数据的识别和保留能力
 */
void testHotDataAccess() {
    std::cout << "\n=== 测试 1: 热点数据访问 ===" << std::endl;

    // 测试参数配置
    const int CAPACITY = 20;         // 缓存容量
    const int OPERATIONS = 500000;   // 总操作次数
    const int HOT_KEYS = 20;         // 热点数据数量
    const int COLD_KEYS = 5000;      // 冷数据数量

    // 初始化各种缓存实例
    RrCache::RLruCache<int, std::string> lru(CAPACITY);
    RrCache::RLfuCache<int, std::string> lfu(CAPACITY);
    RrCache::RArcCache<int, std::string> arc(CAPACITY);
    RrCache::RLruKCache<int, std::string> lruk(CAPACITY, HOT_KEYS + COLD_KEYS, 2);
    RrCache::RLfuCache<int, std::string> lfuAging(CAPACITY, 20000);
    RrCache::RFIFOCache<int, std::string> fifo(CAPACITY);  // 添加 FIFO 缓存

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 使用基类指针数组统一管理不同缓存实现（多态）
    std::array<RrCache::cache_system<int, std::string>*, 6> caches = {&lru, &lfu, &arc, &lruk, &lfuAging, &fifo};
    std::vector<int> hits(6, 0);                          // 记录各算法命中次数
    std::vector<int> get_operations(6, 0);                // 记录各算法 get 操作次数
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging", "FIFO"};

    // 对每种缓存算法执行相同的测试流程
    for (int i = 0; i < caches.size(); ++i) {
        // 阶段1：预热缓存，插入热点数据
        for (int key = 0; key < HOT_KEYS; ++key) {
            std::string value = "value" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 阶段2：执行混合读写操作
        for (int op = 0; op < OPERATIONS; ++op) {
            // 30% 概率执行写操作，70% 概率执行读操作（模拟真实场景）
            bool isPut = (gen() % 100 < 30);
            int key;
            
            // 70% 概率访问热点数据，30% 概率访问冷数据
            if (gen() % 100 < 70) {
                key = gen() % HOT_KEYS;
            } else {
                key = HOT_KEYS + (gen() % COLD_KEYS);
            }
            
            if (isPut) {
                // 写操作：更新或插入数据
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 读操作：获取数据并记录命中情况
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    // 输出测试结果
    printResults("热点数据访问", CAPACITY, get_operations, hits);
}

/**
 * @brief 测试场景2：循环扫描测试
 * 
 * 模拟顺序扫描大量数据的场景，测试各算法对顺序访问模式的处理能力
 */
void testLoopPattern() {
    std::cout << "\n=== 测试 2: 循环扫描 ===" << std::endl;

    // 测试参数配置
    const int CAPACITY = 50;          // 缓存容量
    const int LOOP_SIZE = 500;        // 循环范围大小
    const int OPERATIONS = 200000;    // 总操作次数

    // 初始化各种缓存实例
    RrCache::RLruCache<int, std::string> lru(CAPACITY);
    RrCache::RLfuCache<int, std::string> lfu(CAPACITY);
    RrCache::RArcCache<int, std::string> arc(CAPACITY);
    RrCache::RLruKCache<int, std::string> lruk(CAPACITY, LOOP_SIZE * 2, 2);
    RrCache::RLfuCache<int, std::string> lfuAging(CAPACITY, 3000);
    RrCache::RFIFOCache<int, std::string> fifo(CAPACITY);  // 添加 FIFO 缓存

    // 使用基类指针数组统一管理
    std::array<RrCache::cache_system<int, std::string>*, 6> caches = {&lru, &lfu, &arc, &lruk, &lfuAging, &fifo};
    std::vector<int> hits(6, 0);
    std::vector<int> get_operations(6, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging", "FIFO"};

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 对每种缓存算法执行测试
    for (int i = 0; i < caches.size(); ++i) {
        // 阶段1：预热缓存，加载部分数据
        for (int key = 0; key < LOOP_SIZE / 5; ++key) {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        int current_pos = 0;  // 顺序扫描的当前位置
        
        // 阶段2：执行混合操作
        for (int op = 0; op < OPERATIONS; ++op) {
            // 20% 概率写操作，80% 概率读操作
            bool isPut = (gen() % 100 < 20);
            int key;
            
            // 60% 顺序扫描，30% 随机跳跃，10% 访问范围外数据
            if (op % 100 < 60) {
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE;
            } else if (op % 100 < 90) {
                key = gen() % LOOP_SIZE;
            } else {
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }
            
            if (isPut) {
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    // 输出测试结果
    printResults("循环扫描", CAPACITY, get_operations, hits);
}

/**
 * @brief 测试场景3：工作负载剧烈变化测试
 * 
 * 模拟工作负载在不同阶段发生剧烈变化的场景，测试各算法的自适应能力
 */
void testWorkloadShift() {
    std::cout << "\n=== 测试 3: 工作负载剧烈变化===" << std::endl;

    // 测试参数配置
    const int CAPACITY = 30;            // 缓存容量
    const int OPERATIONS = 80000;       // 总操作次数
    const int PHASE_LENGTH = OPERATIONS / 5;  // 每个阶段的长度

    // 初始化各种缓存实例
    RrCache::RLruCache<int, std::string> lru(CAPACITY);
    RrCache::RLfuCache<int, std::string> lfu(CAPACITY);
    RrCache::RArcCache<int, std::string> arc(CAPACITY);
    RrCache::RLruKCache<int, std::string> lruk(CAPACITY, 500, 2);
    RrCache::RLfuCache<int, std::string> lfuAging(CAPACITY, 10000);
    RrCache::RFIFOCache<int, std::string> fifo(CAPACITY);  // 添加 FIFO 缓存

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 使用基类指针数组统一管理
    std::array<RrCache::cache_system<int, std::string>*, 6> caches = {&lru, &lfu, &arc, &lruk, &lfuAging, &fifo};
    std::vector<int> hits(6, 0);
    std::vector<int> get_operations(6, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging", "FIFO"};

    // 对每种缓存算法执行测试
    for (int i = 0; i < caches.size(); ++i) {
        // 阶段1：预热缓存，插入初始数据
        for (int key = 0; key < 30; ++key) {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 阶段2：多阶段测试，每个阶段有不同的访问模式
        for (int op = 0; op < OPERATIONS; ++op) {
            // 确定当前阶段
            int phase = op / PHASE_LENGTH;
            
            // 每个阶段的读写比例不同
            int putProbability;
            switch (phase) {
                case 0: putProbability = 15; break;  // 阶段1: 热点访问
                case 1: putProbability = 30; break;  // 阶段2: 大范围随机
                case 2: putProbability = 10; break;  // 阶段3: 顺序扫描
                case 3: putProbability = 25; break;  // 阶段4: 局部性随机
                case 4: putProbability = 20; break;  // 阶段5: 混合访问
                default: putProbability = 20;
            }
            
            // 确定是读还是写操作
            bool isPut = (gen() % 100 < putProbability);
            
            // 根据不同阶段选择不同的访问模式生成 key
            int key;
            if (op < PHASE_LENGTH) {
                // 阶段1: 热点访问（5个键）
                key = gen() % 5;
            } else if (op < PHASE_LENGTH * 2) {
                // 阶段2: 大范围随机（400个键）
                key = gen() % 400;
            } else if (op < PHASE_LENGTH * 3) {
                // 阶段3: 顺序扫描（100个键）
                key = (op - PHASE_LENGTH * 2) % 100;
            } else if (op < PHASE_LENGTH * 4) {
                // 阶段4: 局部性随机（5个区域，每个区域15个键）
                int locality = (op / 800) % 5;
                key = locality * 15 + (gen() % 15);
            } else {
                // 阶段5: 混合访问
                int r = gen() % 100;
                if (r < 40) {
                    key = gen() % 5;           // 40% 访问热点
                } else if (r < 70) {
                    key = 5 + (gen() % 45);    // 30% 访问中等范围
                } else {
                    key = 50 + (gen() % 350);  // 30% 访问大范围
                }
            }
            
            if (isPut) {
                // 写操作
                std::string value = "value" + std::to_string(key) + "_p" + std::to_string(phase);
                caches[i]->put(key, value);
            } else {
                // 读操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    // 输出测试结果
    printResults("工作负载剧烈变化", CAPACITY, get_operations, hits);
}

/**
 * @brief 主函数，执行所有测试场景
 */
int main() {
    // 执行三个测试场景
    testHotDataAccess();
    testLoopPattern();
    testWorkloadShift();
    return 0;
}
