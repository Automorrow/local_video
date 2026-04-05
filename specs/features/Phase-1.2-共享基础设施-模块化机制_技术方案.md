# Phase 1.2: 共享基础设施 - 模块化机制 技术方案

## 1. 概述

本文档详细描述 Phase 1.2 "共享基础设施 - 模块化机制" 的技术实现方案。

### 1.1 设计目标
- 实现基于 GCC section 属性的模块自动注册机制
- 提供简洁的宏接口用于模块生命周期管理
- 支持四个生命周期阶段：init → sub → run → exit
- 每个模块包含模块名标识

### 1.2 相关 AC 覆盖
本方案覆盖需求文档中的全部 17 条 AC。

---

## 2. 文件结构

### 2.1 新增文件

```
src/
├── include/
│   └── local_video.h          # 公共头文件（包含模块机制的类型定义）
└── shared/
    └── module/
        ├── module.h            # 模块机制头文件（宏定义）
        └── module.c            # 模块机制实现（遍历执行函数）
```

### 2.2 修改文件
- `src/include/local_video.h` - 添加模块机制相关的类型定义

---

## 3. 核心设计

### 3.1 GCC Section 机制原理

利用 GCC 的 section 属性将结构体放入自定义段，链接器自动生成段的起始和结束符号。

```c
// 示例：将结构体放入自定义段
typedef struct {
    const char *name;
    void (*fn)(void);
} init_entry_t;

__attribute__((section(".embedi_init"), used))
static const init_entry_t _init_my_module = {
    .name = "my_module",
    .fn = my_init_function,
};
```

### 3.2 结构体设计

为每个阶段单独定义结构体，模块可选择性注册各阶段。

#### 3.2.1 初始化阶段结构体
```c
typedef struct {
    const char *name;
    void (*fn)(void);
} module_init_entry_t;
```

#### 3.2.2 订阅阶段结构体
```c
typedef struct {
    const char *name;
    void (*fn)(void);
} module_sub_entry_t;
```

#### 3.2.3 运行阶段结构体
```c
typedef struct {
    const char *name;
    void (*fn)(void);
} module_run_entry_t;
```

#### 3.2.4 退出阶段结构体
```c
typedef struct {
    const char *name;
    void (*fn)(void);
} module_exit_entry_t;
```

→ AC-014

### 3.3 宏设计

#### 3.3.1 MODULE_INIT
```c
#define MODULE_INIT(fn, module_name) \
    __attribute__((used, section(".embedi_init"))) \
    static const module_init_entry_t _init_##fn = { \
        .name = module_name, \
        .fn = fn, \
    }
```
→ AC-001, AC-005, AC-017

#### 3.3.2 MODULE_SUB
```c
#define MODULE_SUB(fn, module_name) \
    __attribute__((used, section(".embedi_sub"))) \
    static const module_sub_entry_t _sub_##fn = { \
        .name = module_name, \
        .fn = fn, \
    }
```
→ AC-002, AC-006, AC-017

#### 3.3.3 MODULE_RUN
```c
#define MODULE_RUN(fn, module_name) \
    __attribute__((used, section(".embedi_run"))) \
    static const module_run_entry_t _run_##fn = { \
        .name = module_name, \
        .fn = fn, \
    }
```
→ AC-003, AC-007, AC-017

#### 3.3.4 MODULE_EXIT
```c
#define MODULE_EXIT(fn, module_name) \
    __attribute__((used, section(".embedi_exit"))) \
    static const module_exit_entry_t _exit_##fn = { \
        .name = module_name, \
        .fn = fn, \
    }
```
→ AC-004, AC-008, AC-017

### 3.4 段遍历执行接口

#### 3.4.1 接口声明（module.h）
```c
void module_init_all(void);
void module_sub_all(void);
void module_run_all(void);
void module_exit_all(void);
```

#### 3.4.2 实现原理（module.c）

利用链接器自动生成的符号：
- `__start_embedi_init` / `__stop_embedi_init`
- `__start_embedi_sub` / `__stop_embedi_sub`
- `__start_embedi_run` / `__stop_embedi_run`
- `__start_embedi_exit` / `__stop_embedi_exit`

→ AC-010, AC-011, AC-012, AC-013

```c
// 声明链接器符号
extern module_init_entry_t __start_embedi_init[];
extern module_init_entry_t __stop_embedi_init[];
extern module_sub_entry_t __start_embedi_sub[];
extern module_sub_entry_t __stop_embedi_sub[];
extern module_run_entry_t __start_embedi_run[];
extern module_run_entry_t __stop_embedi_run[];
extern module_exit_entry_t __start_embedi_exit[];
extern module_exit_entry_t __stop_embedi_exit[];

// 遍历执行初始化函数
void module_init_all(void) {
    module_init_entry_t *entry;
    for (entry = __start_embedi_init; entry < __stop_embedi_init; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

// 遍历执行订阅函数
void module_sub_all(void) {
    module_sub_entry_t *entry;
    for (entry = __start_embedi_sub; entry < __stop_embedi_sub; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

// 遍历执行运行函数
void module_run_all(void) {
    module_run_entry_t *entry;
    for (entry = __start_embedi_run; entry < __stop_embedi_run; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

// 遍历执行退出函数
void module_exit_all(void) {
    module_exit_entry_t *entry;
    for (entry = __start_embedi_exit; entry < __stop_embedi_exit; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}
```

→ AC-009（即使某个函数有问题，循环继续执行下一个）

---

## 4. API 设计

### 4.1 模块注册宏

| 宏名 | 用途 | 段名 | 参数 | 对应 AC |
|------|------|------|------|---------|
| `MODULE_INIT(fn, name)` | 注册初始化函数 | `.embedi_init` | fn: 函数指针, name: 模块名 | AC-001, AC-005, AC-017 |
| `MODULE_SUB(fn, name)` | 注册订阅函数 | `.embedi_sub` | fn: 函数指针, name: 模块名 | AC-002, AC-006, AC-017 |
| `MODULE_RUN(fn, name)` | 注册运行函数 | `.embedi_run` | fn: 函数指针, name: 模块名 | AC-003, AC-007, AC-017 |
| `MODULE_EXIT(fn, name)` | 注册退出函数 | `.embedi_exit` | fn: 函数指针, name: 模块名 | AC-004, AC-008, AC-017 |

**约束**：`fn` 必须是 `void (*)(void)` 类型 → AC-014

### 4.2 生命周期执行函数

| 函数名 | 用途 | 执行顺序 | 对应 AC |
|--------|------|----------|---------|
| `module_init_all()` | 执行所有初始化函数 | 第 1 步 | AC-001, AC-009 |
| `module_sub_all()` | 执行所有订阅函数 | 第 2 步 | AC-002 |
| `module_run_all()` | 执行所有运行函数 | 第 3 步 | AC-003 |
| `module_exit_all()` | 执行所有退出函数 | 第 4 步 | AC-004 |

→ AC-015（执行顺序：init → sub → run → exit）

---

## 5. 使用示例

### 5.1 完整模块定义示例（注册所有阶段）

```c
// src/modules/config/config.c

#include "shared/module/module.h"

static void config_init(void) {
    // 初始化配置...
}

static void config_sub(void) {
    // 订阅通知链...
}

static void config_run(void) {
    // 运行配置模块...
}

static void config_exit(void) {
    // 清理配置模块...
}

MODULE_INIT(config_init, "config");
MODULE_SUB(config_sub, "config");
MODULE_RUN(config_run, "config");
MODULE_EXIT(config_exit, "config");
```

### 5.2 简化模块定义示例（不注册 sub 阶段）

```c
// src/modules/simple/simple.c

#include "shared/module/module.h"

static void simple_init(void) {
    // 初始化...
}

static void simple_run(void) {
    // 运行...
}

static void simple_exit(void) {
    // 清理...
}

MODULE_INIT(simple_init, "simple");
// 不注册 MODULE_SUB，跳过订阅阶段 → AC-016
MODULE_RUN(simple_run, "simple");
MODULE_EXIT(simple_exit, "simple");
```

### 5.3 主程序使用示例

```c
// src/main.c

#include "include/local_video.h"
#include "shared/module/module.h"

int main(int argc, char *argv[]) {
    // 1. 初始化阶段
    module_init_all();

    // 2. 订阅阶段
    module_sub_all();

    // 3. 运行阶段
    module_run_all();

    // 4. 退出阶段
    module_exit_all();

    return 0;
}
```

---

## 6. 边界情况处理

### 6.1 模块初始化失败
- **处理方式**：`module_init_all()` 继续执行其他模块的初始化 → AC-009
- **业务逻辑处理**：各模块自行判断是否运行（不在本模块范围内）

### 6.2 空函数指针
- **处理方式**：遍历前检查 `if (entry->fn)`，跳过空指针

### 6.3 编译器优化
- **处理方式**：使用 `__attribute__((used))` 防止编译器优化掉未引用的结构体

### 6.4 模块不注册某个阶段
- **处理方式**：不调用对应宏即可，该阶段不会有该模块的条目 → AC-016

---

## 7. 范围界定

### 7.1 本次实现
- ✅ 四个模块注册宏（MODULE_INIT/MODULE_SUB/MODULE_RUN/MODULE_EXIT）
- ✅ 四个阶段的结构体定义
- ✅ 四个段遍历执行函数
- ✅ GCC section 属性自动注册机制
- ✅ 链接器符号访问
- ✅ 支持模块名标识
- ✅ 支持模块选择性注册各阶段

### 7.2 不实现（与需求一致）
- ❌ 模块优先级控制
- ❌ 模块依赖关系管理
- ❌ 模块初始化失败的全局统一处理
- ❌ 模块动态加载/卸载

---

## 8. AC 覆盖对照表

| AC 编号 | 验收标准 | 技术方案对应点 |
|---------|----------|----------------|
| AC-001 | 初始化阶段执行所有 MODULE_INIT 函数 | `module_init_all()` 遍历 `.embedi_init` 段 |
| AC-002 | 订阅阶段执行所有 MODULE_SUB 函数 | `module_sub_all()` 遍历 `.embedi_sub` 段 |
| AC-003 | 运行阶段执行所有 MODULE_RUN 函数 | `module_run_all()` 遍历 `.embedi_run` 段 |
| AC-004 | 退出阶段执行所有 MODULE_EXIT 函数 | `module_exit_all()` 遍历 `.embedi_exit` 段 |
| AC-005 | MODULE_INIT 模块信息（含模块名）放入 `.embedi_init` 段 | `MODULE_INIT` 宏使用 `section(".embedi_init")`，结构体包含 name 字段 |
| AC-006 | MODULE_SUB 模块信息（含模块名）放入 `.embedi_sub` 段 | `MODULE_SUB` 宏使用 `section(".embedi_sub")`，结构体包含 name 字段 |
| AC-007 | MODULE_RUN 模块信息（含模块名）放入 `.embedi_run` 段 | `MODULE_RUN` 宏使用 `section(".embedi_run")`，结构体包含 name 字段 |
| AC-008 | MODULE_EXIT 模块信息（含模块名）放入 `.embedi_exit` 段 | `MODULE_EXIT` 宏使用 `section(".embedi_exit")`，结构体包含 name 字段 |
| AC-009 | 某个模块初始化失败，继续执行其他模块 | `module_init_all()` 用 for 循环，不提前退出 |
| AC-010 | 链接器生成 `.embedi_init` 段起始/结束符号 | 使用 `__start_embedi_init` / `__stop_embedi_init` |
| AC-011 | 链接器生成 `.embedi_sub` 段起始/结束符号 | 使用 `__start_embedi_sub` / `__stop_embedi_sub` |
| AC-012 | 链接器生成 `.embedi_run` 段起始/结束符号 | 使用 `__start_embedi_run` / `__stop_embedi_run` |
| AC-013 | 链接器生成 `.embedi_exit` 段起始/结束符号 | 使用 `__start_embedi_exit` / `__stop_embedi_exit` |
| AC-014 | 注册函数类型必须是 `void (*)(void)` | 结构体定义和宏约束 |
| AC-015 | 执行顺序：init → sub → run → exit | 主程序按此顺序调用四个 `*_all()` 函数 |
| AC-016 | 模块不注册某个阶段时，该阶段不执行该模块 | 不调用对应宏即可，该阶段段中没有该模块条目 |
| AC-017 | 宏必须传入模块名参数 | 宏定义要求第二个参数为 module_name |
