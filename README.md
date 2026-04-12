# LocalVideoServer

轻量级本地视频服务器，支持在手机、平板和电脑上通过浏览器观看和管理本地视频文件。

## 技术栈

- **后端**: C99 + POSIX threads
- **数据库**: SQLite3（配置 + 视频元数据 + 播放历史 + 目录缓存）
- **构建系统**: CMake
- **Web 服务器**: 自实现 HTTP/1.1，支持范围请求与流式播放
- **前端**: 原生 ES Modules（零构建工具），支持 PWA

## 功能特性

### 核心功能
- 视频文件扫描与元数据管理（实时监控文件变化）
- 视频列表展示、搜索与无限滚动加载
- 按目录分类浏览
- 视频播放（支持范围请求拖动播放、恢复上次播放位置）
- 播放历史记录（连续播放同一视频仅保留最新记录）
- 收藏功能（带收藏标记）
- **加权随机播放** — 播放次数越低的视频被随机选中的概率越高
- 目录黑名单管理（标记而非删除，可快速恢复）
- 数据库为空时自动扫描与引导设置

### 前端体验
- **移动端优先**：响应式布局、底部固定导航栏、模态框抽屉化/全屏化
- **目录选择器**：支持从盘符根目录自由选择（Windows 下可直接切换驱动器）
- 搜索框折叠（移动端）、一键返回顶部悬浮按钮
- Toast 通知提示、加载动画、空状态引导
- 键盘快捷键：左右箭头快进/快退 10 秒、`F` 全屏、`M` 静音、`N` 下一个

### 安全与稳定
- XSS 防护 — 所有动态内容均经过 `escapeHtml` 转义
- 路径遍历防护 — 视频流与目录解析受白名单限制
- CORS 限制为 `localhost`
- 配置持久化迁移至 SQLite `settings` 表

## 编译

```bash
mkdir -p build
cd build
cmake ..
make
```

Windows（MinGW）参见 `README-WINDOWS.md`。

## 运行

```bash
cd build/bin
./local_video --port 8088 --video-dir /path/to/videos --web-root ../../web/static
```

> 默认端口为 **8088**（自动回退 8089–8098）。配置已持久化到 SQLite，首次设置后重启可自动读取上次目录。

## 访问

在浏览器中打开: `http://localhost:8088`

## 目录说明

- `src/main.c`: 程序入口，负责模块生命周期调度
- `src/modules/`: 业务模块（API、HTTP 服务、数据库、扫描、配置）
- `src/shared/`: 共享基础设施（module/list/notifier/log/thread/json/platform）
- `src/include/`: 对多模块共享的公共头文件
- `web/static/`: 前端静态资源（ES Modules 拆分：api.js / common.js / modal.js / player.js / settings.js / app.js）
- `tests/unit/`: 单元测试
- `specs/`: 项目规格文档与开发规范
- `docs/`: 补充文档（模块机制、Windows 说明、开发记录）
- `tools/`: 辅助工具与非主运行时脚本

## 最近更新

- **2025-04**: 移动端 UI 重构、底部导航、模态框全屏/抽屉化
- **2025-04**: 加权随机算法 — 基于 `play_count` 分层桶偏移随机，避免全表 `ORDER BY RANDOM()`
- **2025-04**: 设置页目录浏览修复 — 支持从 Root 重新选择任意盘符
- **2025-04**: 历史记录去重优化 — 同一视频连续播放仅更新记录，被其它视频隔开则保留多条
- **2025-04**: 配置迁移 — 移除 `local_video.cfg`，统一使用 SQLite `settings` 表

## 开发入口

- 首次了解项目：阅读 `README.md`
- 了解构建与发布：阅读 `BUILD.md` 与 `README-WINDOWS.md`
- 了解目录与边界：阅读 `specs/项目结构.md`
- 了解代码规范：阅读 `specs/开发规范.md`
- 了解模块自注册机制：阅读 `docs/模块系统.md`

## 仓库分层

1. **主产品主线**：`src/`、`tests/`、`CMakeLists.txt`、`linker.ld`、`web/static/`
2. **项目文档层**：`README.md`、`BUILD.md`、`README-WINDOWS.md`、`docs/`、`specs/`
3. **辅助工具层**：`tools/`

## ⚖️ 法律免责声明

本项目仅供**教育和合法用途**。用户须自行承担遵守相关法律法规的责任。作者不对任何滥用或非法使用行为承担责任。

## 许可证

MIT License
