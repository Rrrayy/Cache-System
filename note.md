## 1. size_t
 size_t是无符号整数类型,只能是0或正数，不能为负，可用作表示容器大小，数组索引，字符串长度，内存大小，不能与负数作比较，也尽量不和有符号类型作比较 ,  
 需要头文件<"cstddef>
 >int(有符号) unsigned_int(无符号)  
 >int i < vector<int>.size() 改写成 >size_t i < vector<int>.size()
## 2.智能指针
 >头文件<memory>
 weak_ptr,shared_ptr,unique_otr实际上都是智能的指针类，不用手动释放内存，内存自动释放
 | 特性 | unique_ptr | shared_ptr | weak_ptr |
|:---|:---:|:---:|:---|
| 所有权 | 独占 | 共享 | 无（弱引用）|
| 可复制 | ❌ 否 | ✅ 是 | ✅ 是 |
| 可移动 | ✅ 是 | ✅ 是 | ✅ 是 |
| 引用计数 | 无 | 有 | 无 |
| 内存开销 | 最小 | 中等 | 最小 |
| 核心用途 | 唯一拥有 | 共享拥有 | 打破循环引用 |
>项目中双向链表中前后指针的使用  
next_ 用 shared_ptr：拥有下一个节点  
 prev_ 用 weak_ptr：只是观察前一个节点，打破循环引用


 如果两个都用 shared_ptr

节点A ◄───── shared_ptr ─────► 节点B
  │                              │
  └──── shared_ptr ◄── shared_ptr ─┘
  
循环引用！
- A 的 next_ 指向 B（B 计数+1）
- B 的 prev_ 指向 A（A 计数+1）
- 互相等待对方先释放 → 内存泄漏！
- 
改用 weak_ptr 打破循环

- 节点A ◄───── shared_ptr ─────► 节点B
  │                              │
  └──── weak_ptr ◄── shared_ptr ─┘

- A 的 next_ 指向 B（B 计数+1）
- B 的 prev_ 指向 A（A 计数不变！）

没有循环引用！
### std::make_shared
>创建shared_ptr的高效方式
```
//写法1-默认初始化
auto p1=std::make_shared<T>();
//写法2-自定义类型
auto p2=std::make_shared<T>(arg1,arg2);
```
### weak_ptr的lock用法
>lock先weak_ptr转为shared_ptr,这样才能安全使用，因为weak_ptr没有->操作符，如果对象已销毁，则返回nullptr
```
void removeNode(NodePtr node) {
    // 直接 lock()，然后检查返回值
    auto prev = node->prev_.lock();
    if (prev && node->next_) {
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
    }
}
```
### 3.expired用法
```
.expired() //检查指向的对象是否还活着
返回true说明已经被销毁  
项目中!node->prev_.expired() && node->next_检查指向的对象还活着，才能安全使用
```

## mutex互斥锁
>防止多个线程同时访问同一份数据，避免数据错乱。
创建lock_guard自动加锁，运行代码时锁状态保持，离开作用域自动解锁
```
std::lock_guard<std::mutex> lock(mutex_);
```
## 4.纯虚函数和虚函数区别
| 方面 | 虚函数 | 纯虚函数 |
|:---|:---|:---|
| 语法 | `virtual void func() {}` | `virtual void func() = 0` |
| 有实现 | ✅ 有 | ❌ 无 |
| 子类必须重写 | ❌ 不一定 | ✅ 必须 |
| 能创建对象 | ✅ 能 | ❌ 不能 |
| 用途 | 提供默认实现 | 定义接口，强制子类实现 |