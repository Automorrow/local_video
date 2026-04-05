# 自动构建说明

本项目使用 GitHub Actions 自动编译 Linux 和 Windows 版本。

## 触发构建

### 方式 1：推送代码
当你推送代码到 `main` 或 `master` 分支时，会自动触发构建。

```bash
git add .
git commit -m "你的修改"
git push origin main
```

### 方式 2：手动触发
1. 打开 GitHub 仓库页面
2. 点击 **Actions** 标签
3. 选择 **Build** 工作流
4. 点击 **Run workflow** 按钮

## 下载编译结果

构建完成后，可以在以下位置下载：

### 方式 1：GitHub Actions 页面
1. 打开 GitHub 仓库页面
2. 点击 **Actions** 标签
3. 点击最新的工作流运行记录
4. 在 **Artifacts** 部分下载：
   - `local_video-linux` - Linux 可执行文件
   - `local_video-windows` - Windows 可执行文件（包含 DLL 和 web 目录）

### 方式 2：Release 页面（推荐）
每次推送到 main 分支后，会自动创建 Release 包：
- `local_video-linux.tar.gz`
- `local_video-windows.zip`

## 本地构建

### Linux
```bash
./build-linux.sh
# 输出: build-linux/bin/local_video
```

### Windows (交叉编译)
```bash
./build-windows.sh
# 输出: build-windows/bin/local_video.exe
```

## Windows 版本使用说明

下载 `local_video-windows.zip` 后：

1. 解压到任意目录
2. 双击 `start.bat` 启动，或命令行运行：
   ```cmd
   local_video.exe --port 8080 --video-dir C:\path\to\videos
   ```
3. 浏览器访问 `http://localhost:8080`

**注意**：Windows 版本暂不支持视频目录自动扫描功能（需要手动添加视频到数据库或使用 Linux 版本扫描后复制数据库）。
