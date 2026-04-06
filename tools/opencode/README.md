# OpenCode 辅助工具

本目录用于存放与 LocalVideoServer 主产品运行时解耦的 OpenCode / 模型配置辅助脚本。

这些文件：

- **不参与** C99 服务端构建
- **不参与** CTest
- 仅服务开发者本地工具链或模型配置整理

## 文件说明

- `package.json` / `package-lock.json`：Node 依赖声明
- `generate_config.js`：生成一版 OpenCode 配置
- `generate_correct_config.js`：生成另一版兼容配置
- `generate_proper_config.js`：生成较完整的 provider 配置
- `parse_models.js`：检查模型列表输入

## 输入文件

上述脚本默认读取当前目录下的 `models` 文件，也可通过命令行参数传入自定义路径：

```bash
node parse_models.js ./models
node generate_proper_config.js ./models
```

`models` 属于本地工具输入，不属于项目源码，已在仓库忽略规则中排除。
