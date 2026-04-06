# LocalVideoServer

轻量级本地视频服务器，支持在不同设备上通过网页观看和管理本地视频文件。

## 技术栈

- **后端**: C99
- **数据库**: SQLite3
- **构建系统**: CMake
- **Web服务器**: 自实现 HTTP/1.1

## 功能特性

- 视频文件扫描与元数据管理（实时监控文件变化）
- 视频列表展示与搜索（支持防抖搜索）
- 按目录分类
- 视频播放（支持范围请求拖动播放）
- 播放历史记录（支持继续上次播放位置）
- 收藏功能（带收藏标记）
- 随机播放
- 目录黑名单管理（标记而非删除，可快速恢复）
- 数据库为空时自动扫描
- Toast 通知提示
- 加载动画
- 键盘快捷键支持
- 局域网访问支持

## 编译

```bash
mkdir -p build
cd build
cmake ..
make
```

## 运行

```bash
cd build/bin
./local_video --port 8080 --video-dir /path/to/videos --web-root ../../web/static
```

## 访问

在浏览器中打开: `http://localhost:8080`

## 目录说明

- `src/main.c`: 程序入口，负责模块生命周期调度
- `src/modules/`: 业务模块（API、HTTP 服务、数据库、扫描、配置）
- `src/shared/`: 共享基础设施（module/list/notifier/log/thread/json/platform）
- `src/include/`: 对多模块共享的公共头文件
- `web/static/`: 前端静态资源唯一入口
- `tests/unit/`: 单元测试
- `specs/`: 项目规格文档与开发规范
- `docs/`: 补充文档（模块机制、Windows 说明、开发记录）
- `tools/`: 辅助工具与非主运行时脚本

## 仓库分层

仓库当前按以下三层维护：

1. **主产品主线**
   - `src/`、`tests/`、`CMakeLists.txt`、`linker.ld`、`web/static/`
   - 这是 LocalVideoServer 的 C99 + CMake + SQLite3 主交付面。
2. **项目文档层**
   - `README.md`、`BUILD.md`、`README-WINDOWS.md`、`docs/`、`specs/`
   - 用于解释架构、构建、平台差异和开发规则。
3. **辅助工具层**
   - `tools/`
   - 存放与主运行时解耦的开发辅助脚本，不作为服务端主产品的一部分。

## 开发入口

- 首次了解项目：阅读 `README.md`
- 了解构建与发布：阅读 `BUILD.md` 与 `README-WINDOWS.md`
- 了解目录与边界：阅读 `specs/项目结构.md`
- 了解代码规范：阅读 `specs/开发规范.md`
- 了解模块自注册机制：阅读 `docs/模块系统.md`

## 许可证

MIT License
