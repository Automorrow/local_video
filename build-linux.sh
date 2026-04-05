#!/bin/bash
# Linux 本地构建脚本

set -e

echo "=== LocalVideoServer Linux Build ==="
echo ""

# 创建构建目录
BUILD_DIR="build-linux"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置 CMake
echo "=== Configuring for Linux ==="
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
echo "=== Building ==="
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "输出文件: $PWD/bin/local_video"
echo ""
echo "运行方式:"
echo "  ./bin/local_video --port 8080 --video-dir /path/to/videos"
