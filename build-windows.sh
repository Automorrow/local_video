#!/bin/bash
# Windows 交叉编译脚本 (MinGW-w64)

set -e

echo "=== LocalVideoServer Windows Build ==="
echo ""

# 检查工具链
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "错误: MinGW-w64 未安装"
    echo "请运行: sudo apt-get install -y mingw-w64 cmake"
    exit 1
fi

# 检查 SQLite3 库
if [ ! -f "third_party/sqlite3/libsqlite3.a" ]; then
    echo "错误: SQLite3 库未找到"
    echo "请确保 third_party/sqlite3/libsqlite3.a 存在"
    echo "可以从 sqlite-dll-win-x64-*.zip 生成:"
    echo "  x86_64-w64-mingw32-dlltool -d sqlite3.def -l libsqlite3.a"
    exit 1
fi

if [ ! -f "third_party/sqlite3/sqlite3.h" ]; then
    echo "错误: SQLite3 头文件未找到"
    echo "请确保 third_party/sqlite3/sqlite3.h 存在"
    exit 1
fi

# 创建构建目录
BUILD_DIR="build-windows"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置 CMake
echo "=== Configuring for Windows ==="
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 构建
echo "=== Building ==="
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "输出文件: $PWD/bin/local_video.exe"
echo ""
echo "运行方式 (在 Windows 上):"
echo "  1. 复制以下文件到 Windows 目录:"
echo "     - bin/local_video.exe"
echo "     - third_party/sqlite3/sqlite3.dll"
echo "     - web/ 目录"
echo "  2. 在 Windows 命令行运行:"
echo "     local_video.exe --port 8080 --video-dir C:\\path\\to\\videos"
echo ""
echo "注意: Windows 版本暂不支持视频目录自动扫描功能"
echo "      需要手动将视频信息添加到数据库"
