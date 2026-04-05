# Phase 1.2: 共享基础设施 - 模块化机制

## 1. 功能概述
实现基于 GCC section 属性的模块生命周期管理机制，提供 `MODULE_INIT`、`MODULE_SUB`、`MODULE_RUN`、`MODULE_EXIT` 宏（接受模块名参数），使用结构体封装模块信息，实现模块的自动注册和生命周期管理。模块可选择性注册各阶段。

## 2. 核心流程

### 2.1 模块生命周期
1. **初始化阶段 (Init)**：执行所有 `MODULE_INIT` 注册的函数
2. **订阅阶段 (Sub)**：执行所有 `MODULE_SUB` 注册的函数
3. **运行阶段 (Run)**：执行所有 `MODULE_RUN` 注册的函数
4. **退出阶段 (Exit)**：执行所有 `MODULE_EXIT` 注册的函数

### 2.2 关键说明
- 即使某个模块初始化失败，也继续执行其他模块的初始化
- 模块初始化失败时，业务逻辑模块需自行判断是否运行
- `MODULE_SUB` 用于在初始化完成后订阅通知链
- 模块可选择性注册各阶段（例如：可以只注册 init/run/exit，不注册 sub）
- 每个注册宏都需要传入模块名参数

---

## 3. 验收标准 (Acceptance Criteria)

### 3.1 Happy Path (正常流程)

**AC-001**: Given 程序正常启动，When 进入初始化阶段，Then 所有 `MODULE_INIT` 注册的函数按顺序执行。

**AC-002**: Given 所有模块初始化完成，When 进入订阅阶段，Then 所有 `MODULE_SUB` 注册的函数按顺序执行。

**AC-003**: Given 订阅阶段完成，When 进入运行阶段，Then 所有 `MODULE_RUN` 注册的函数按顺序执行。

**AC-004**: Given 程序正常退出，When 进入退出阶段，Then 所有 `MODULE_EXIT` 注册的函数按顺序执行。

**AC-005**: Given 一个模块使用 `MODULE_INIT` 注册初始化函数（传入模块名），When 编译链接程序，Then 该模块信息（包含模块名和函数指针）被自动放入 `.embedi_init` 段。

**AC-006**: Given 一个模块使用 `MODULE_SUB` 注册订阅函数（传入模块名），When 编译链接程序，Then 该模块信息（包含模块名和函数指针）被自动放入 `.embedi_sub` 段。

**AC-007**: Given 一个模块使用 `MODULE_RUN` 注册运行函数（传入模块名），When 编译链接程序，Then 该模块信息（包含模块名和函数指针）被自动放入 `.embedi_run` 段。

**AC-008**: Given 一个模块使用 `MODULE_EXIT` 注册退出函数（传入模块名），When 编译链接程序，Then 该模块信息（包含模块名和函数指针）被自动放入 `.embedi_exit` 段。

### 3.2 Edge Cases (边界情况)

**AC-009**: Given 某个模块的初始化函数执行过程中出现问题，When 继续执行初始化阶段，Then 其他模块的初始化函数仍然正常执行。

**AC-010**: Given 程序编译时包含多个模块的初始化函数，When 链接完成，Then 链接器自动生成 `.embedi_init` 段的起始和结束符号。

**AC-011**: Given 程序编译时包含多个模块的订阅函数，When 链接完成，Then 链接器自动生成 `.embedi_sub` 段的起始和结束符号。

**AC-012**: Given 程序编译时包含多个模块的运行函数，When 链接完成，Then 链接器自动生成 `.embedi_run` 段的起始和结束符号。

**AC-013**: Given 程序编译时包含多个模块的退出函数，When 链接完成，Then 链接器自动生成 `.embedi_exit` 段的起始和结束符号。

**AC-016**: Given 一个模块只注册了 init/run/exit 而没有注册 sub，When 进入订阅阶段，Then 该模块的订阅函数不被执行。

### 3.3 Business Rules (业务规则)

**AC-014**: Given 任何情况下，When 使用 `MODULE_INIT`、`MODULE_SUB`、`MODULE_RUN`、`MODULE_EXIT` 宏，Then 被注册的函数类型必须是 `void (*)(void)`。

**AC-015**: Given 程序正常启动，When 执行各阶段函数，Then 执行顺序为：init → sub → run → exit。

**AC-017**: Given 任何情况下，When 使用 `MODULE_INIT`、`MODULE_SUB`、`MODULE_RUN`、`MODULE_EXIT` 宏，Then 必须传入模块名作为第二个参数。

---

## 4. 范围界定

### 4.1 本次做什么
- 实现 `MODULE_INIT(fn, name)` 宏，用于注册模块初始化函数
- 实现 `MODULE_SUB(fn, name)` 宏，用于注册通知链订阅函数
- 实现 `MODULE_RUN(fn, name)` 宏，用于注册模块运行函数
- 实现 `MODULE_EXIT(fn, name)` 宏，用于注册模块退出函数
- 实现遍历各自定义段并执行函数的接口
- 使用 GCC section 属性和链接器符号实现自动注册机制
- 使用结构体封装模块信息（包含模块名和函数指针）
- 支持模块选择性注册各阶段

### 4.2 本次不做什么
- 不实现模块优先级控制
- 不实现模块依赖关系管理
- 不实现模块初始化失败的全局统一处理机制
- 不实现模块动态加载/卸载功能
