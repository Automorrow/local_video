# Windows 构建指南

## 方案一：MinGW-w64 交叉编译（推荐）

在 WSL/Ubuntu 中编译出 Windows 可执行文件。

### 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y mingw-w64 cmake
```

### 2. 获取 Windows 版 SQLite3

```bash
# 下载预编译的 Windows SQLite3
mkdir -p /tmp/sqlite-win
cd /tmp/sqlite-win
wget https://www.sqlite.org/2024/sqlite-dll-win64-x64-3450000.zip
wget https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
unzip sqlite-dll-win64-x64-3450000.zip
unzip sqlite-amalgamation-3450000.zip
```

### 3. 构建

```bash
cd /home/y/local_video
mkdir -p build-win
cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-toolchain.cmake \
         -DSQLITE3_ROOT=/tmp/sqlite-win
make
```

输出文件：`build-win/bin/local_video.exe`

## 方案二：MSYS2 (Windows 本地编译)

在 Windows 上安装 MSYS2，然后：

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-sqlite3
cd /path/to/local_video
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

## 已知问题

当前代码使用了一些 POSIX 特定 API，需要适配：
- `unistd.h` → Windows 替代
- `sys/socket.h` → Winsock2
- `pthread` → Windows threads 或 pthread-win32
- `lstat()` → Windows 等效函数
- `strcasecmp()` → `_stricmp()` (Windows)

完整移植需要修改源代码添加条件编译。
