# LocalVideoServer 接口规范文档

## 0. 文档概述

### 0.1 接口分类

本规范定义 LocalVideoServer 的全部接口契约，分为三大类：

| 类别 | 调用方 | 说明 |
|------|--------|------|
| **A. RESTful API（外部接口）** | Web 前端 / HTTP 客户端 | 基于 HTTP/1.1 的 JSON 接口 |
| **B. 模块间内部接口（C 函数）** | 业务模块之间 | 头文件声明的 C 函数与数据结构 |
| **C. 事件通知接口（Notifier Chain）** | 发布者 → 订阅者 | 基于通知链的异步事件契约 |

### 0.2 技术约束

- **协议**: HTTP/1.1
- **数据格式**: JSON（REST API）、C 结构体（内部接口）
- **字符编码**: UTF-8
- **基础 URL**: `http://localhost:8080`
- **API 前缀**: `/api/`

---

## A 篇：RESTful API（外部接口）

### A.1 通用规范

#### A.1.1 请求格式

```
HTTP/{method} {path} HTTP/1.1\r\n
Host: {host}\r\n
Content-Type: application/json\r\n
Connection: close\r\n
\r\n
{body}
```

**支持的 HTTP 方法**: GET、POST、DELETE

**请求头约定**:

| 头部 | 必填 | 示例值 | 说明 |
|------|------|--------|------|
| `Host` | 是 | `localhost:8080` | RFC 7230 要求 |
| `Content-Type` | POST 必填 | `application/json` | 仅 POST/PUT 请求需要 |
| `Range` | 可选 | `bytes=0-` | 视频文件范围请求 |

**查询参数编码**: URL 编码（`encodeURIComponent`），服务端自动解码。

#### A.1.2 响应格式

**成功响应 — 数据列表（GET 列表类）**:

HTTP 状态码: `200 OK`
Content-Type: `application/json`

```json
[
  { "...": "..." },
  { "...": "..." }
]
```

> 返回 JSON 数组，空列表返回 `[]`。不使用分页包装对象（数据量小，无需分页）。

**成功响应 — 操作确认（POST/DELETE）**:

HTTP 状态码: `200 OK` 或 `201 Created`

```json
{
  "success": true
}
```

带附加数据时：

```json
{
  "success": true,
  "data": { "...": "..." }
}
```

**错误响应**:

```json
{
  "success": false,
  "error": "人类可读的错误描述"
}
```

#### A.1.3 HTTP 状态码规范

| 状态码 | 含义 | 使用场景 |
|--------|------|----------|
| `200 OK` | 成功 | GET 返回数据、POST/DELETE 操作成功 |
| `206 Partial Content` | 部分内容 | 视频文件 Range 请求 |
| `400 Bad Request` | 参数错误 | 缺少必填参数、参数格式无效、路径非法 |
| `404 Not Found` | 资源不存在 | 视频 ID 无效、黑名单条目不存在 |
| `409 Conflict` | 资源冲突 | 重复添加收藏、目录已在黑名单 |

#### A.1.4 CORS 策略

所有 API 响应包含头部：
```
Access-Control-Allow-Origin: *
```

原因：局域网使用场景，无需限制来源。

#### A.1.5 视频流式传输协议

视频文件通过 `/video/{relative_path}` 端点提供，遵循 HTTP Range Requests 规范 (RFC 7233)。

**完整文件请求**:

```
GET /video/movies/example.mp4 HTTP/1.1

→ HTTP/1.1 200 OK
   Content-Type: video/mp4
   Content-Length: {file_size}
   Accept-Ranges: bytes
   Connection: close

   {binary file content}
```

**Range 请求（拖动播放）**:

```
GET /video/movies/example.mp4 HTTP/1.1
Range: bytes=1024000-

→ HTTP/1.1 206 Partial Content
   Content-Type: video/mp4
   Content-Range: bytes 1024000-{file_size-1}/{file_size}
   Content-Length: {remaining_bytes}
   Accept-Ranges: bytes
   Connection: close

   {binary file content from offset 1024000}
```

**中间范围请求**:

```
Range: bytes=1024000-2048000

→ HTTP/1.1 206 Partial Content
   Content-Range: bytes 1024000-2048000/{file_size}
   Content-Length: 1024001
```

**安全约束**:
- 路径必须以视频根目录为前缀（禁止 `..` 遍历攻击）
- 路径规范化后校验
- 仅允许访问 `.mp4` 和 `.webm` 文件

---

### A.2 视频资源接口

#### A.2.1 获取视频列表

**端点**: `GET /api/videos`

**描述**: 获取所有非黑名单视频列表，支持搜索、分类筛选和分页。

**Query Parameters:**

| 参数 | 类型 | 必填 | 默认值 | 约束 | 说明 |
|------|------|------|--------|------|------|
| `search` | string | 否 | - | 最大 256 字符 | 按标题模糊搜索（SQL LIKE） |
| `category` | string | 否 | - | 最大 256 字符 | 按分类名精确匹配 |
| `limit` | integer | 否 | 100 | 1~500 | 每页返回条数上限 |
| `offset` | integer | 否 | 0 | ≥ 0 | 跳过前 N 条记录 |

> `search` 与 `category` 互斥，同时传递时优先使用 `search`。
> `limit` 和 `offset` 用于分页，不传时默认返回全部结果（兼容旧客户端）。

**成功响应 (200)**:

```json
[
  {
    "id": 1,
    "title": "Example Movie.mp4",
    "path": "/videos/movies/Example Movie.mp4",
    "category": "movies",
    "size": 1073741824
  }
]
```

**响应字段定义**:

| 字段 | 类型 | 说明 | 来源 |
|------|------|------|------|
| `id` | integer | 视频唯一标识 | `videos.id` |
| `title` | string | 文件名（含扩展名） | `videos.title` |
| `path` | string | 文件系统绝对路径 | `videos.path` |
| `category` | string | 所属分类（父目录名） | `videos.category` |
| `size` | integer | 文件大小（字节） | `videos.size` |

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| 数据库查询失败 | 500 | `{"success":false,"error":"Internal error"}` |

**示例**:

```bash
# 所有视频
curl http://localhost:8080/api/videos

# 搜索
curl "http://localhost:8080/api/videos?search=movie"

# 按分类筛选
curl "http://localhost:8080/api/videos?category=documentaries"
```

---

#### A.2.2 获取随机视频

**端点**: `GET /api/videos/random`

**描述**: 从所有非黑名单视频中随机获取一个。

**Query Parameters**: 无

**成功响应 (200)**:

```json
[
  {
    "id": 42,
    "title": "Random Pick.webm",
    "path": "/videos/tv_shows/Random Pick.webm",
    "category": "tv_shows",
    "size": 536870912
  }
]
```

> 始终返回长度为 1 的数组（与列表接口保持一致）。无视频时返回 `[]`。

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| 数据库无视频记录 | 200 | `[]`（空数组） |

**示例**:

```bash
curl http://localhost:8080/api/videos/random
```

---

#### A.2.3 获取分类列表

**端点**: `GET /api/categories`

**描述**: 获取所有存在视频的分类（去重）。

**Query Parameters**: 无

**成功响应 (200)**:

```json
[
  { "name": "movies" },
  { "name": "tv_shows" },
  { "name": "documentaries" }
]
```

**响应字段定义**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 分类名称（对应视频所在目录名） |

> 分类按名称升序排列。无视频时返回 `[]`。

**示例**:

```bash
curl http://localhost:8080/api/categories
```

---

### A.3 播放历史接口

#### A.3.1 获取播放历史

**端点**: `GET /api/history`

**描述**: 获取所有播放历史记录，按播放时间倒序排列。

**Query Parameters**: 无

**成功响应 (200)**:

```json
[
  {
    "id": 1,
    "video_id": 42,
    "title": "Example Movie.mp4",
    "path": "/videos/movies/Example Movie.mp4",
    "position": 125
  }
]
```

**响应字段定义**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | 历史记录唯一 ID |
| `video_id` | integer | 关联的视频 ID |
| `title` | string | 视频标题（快照，即使原视频被删除也保留） |
| `path` | string | 视频路径（快照） |
| `position` | integer | 上次播放位置（**秒**，与 HTML5 Video API currentTime 一致） |

> 注意：`title` 和 `path` 是写入时的快照，用于在视频被删除后仍能显示历史信息。

**示例**:

```bash
curl http://localhost:8080/api/history
```

---

#### A.3.2 添加/更新播放历史

**端点**: `POST /api/history`

**描述**: 记录或更新视频播放位置。同一 video_id 的记录执行 UPSERT（存在则更新 position，不存在则插入）。

**Request Body (JSON):**

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `video_id` | integer | 是 | > 0 | 关联的视频 ID |
| `position` | integer | 是 | ≥ 0 | 当前播放位置（**秒**） |

**请求示例**:

```json
{
  "video_id": 42,
  "position": 125
}
```

**成功响应 (200)**:

```json
{
  "success": true
}
```

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| 缺少 video_id 或 position | 400 | `{"success":false,"error":"Missing required fields"}` |
| video_id 不存在 | 400 | `{"success":false,"error":"Video not found"}` |
| Body 不是合法 JSON | 400 | `{"success":false,"error":"Invalid JSON body"}` |

**示例**:

```bash
curl -X POST http://localhost:8080/api/history \
  -H "Content-Type: application/json" \
  -d '{"video_id":42,"position":125}'
```

---

#### A.3.3 删除单条历史记录

**端点**: `DELETE /api/history/:id`

**描述**: 删除指定 ID 的单条播放历史记录。

**Path Parameters:**

| 参数 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `id` | integer | 是 | > 0 | 历史记录的 `id`（非 video_id） |

**成功响应 (200)**:

```json
{
  "success": true
}
```

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| id 格式无效 | 400 | `{"success":false,"error":"Invalid id"}` |
| 历史记录不存在 | 404 | `{"success":false,"error":"History entry not found"}` |

**示例**:

```bash
curl -X DELETE http://localhost:8080/api/history/1
```

---

#### A.3.4 清空播放历史

**端点**: `DELETE /api/history`

**描述**: 删除所有播放历史记录。

**Request Body**: 无

**成功响应 (200)**:

```json
{
  "success": true
}
```

**示例**:

```bash
curl -X DELETE http://localhost:8080/api/history
```

---

### A.4 收藏接口

#### A.4.1 获取收藏列表

**端点**: `GET /api/favorites`

**描述**: 获取当前用户的所有收藏视频。

**Query Parameters**: 无

**成功响应 (200)**:

```json
[
  {
    "id": 1,
    "video_id": 42,
    "title": "Favorite Movie.mp4",
    "path": "/videos/movies/Favorite Movie.mp4"
  }
]
```

**响应字段定义**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | 收藏条目 ID |
| `video_id` | integer | 关联的视频 ID |
| `title` | string | 视频标题（快照） |
| `path` | string | 视频路径（快照） |

**示例**:

```bash
curl http://localhost:8080/api/favorites
```

---

#### A.4.2 添加收藏

**端点**: `POST /api/favorites`

**描述**: 将指定视频加入收藏列表。

**Request Body (JSON):**

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `video_id` | integer | 是 | > 0 | 要收藏的视频 ID |

**请求示例**:

```json
{
  "video_id": 42
}
```

**成功响应 (200)**:

```json
{
  "success": true
}
```

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| video_id 已在收藏中 | 409 | `{"success":false,"error":"Already in favorites"}` |
| video_id 不存在 | 400 | `{"success":false,"error":"Video not found"}` |
| 缺少 video_id | 400 | `{"success":false,"error":"Missing video_id"}` |

**示例**:

```bash
curl -X POST http://localhost:8080/api/favorites \
  -H "Content-Type: application/json" \
  -d '{"video_id":42}'
```

---

#### A.4.3 取消收藏

**端点**: `DELETE /api/favorites/:video_id`

**描述**: 从收藏列表中移除指定视频。

**Path Parameters:**

| 参数 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `video_id` | integer | 是 | > 0 | 要取消收藏的视频 ID |

**成功响应 (200)**:

```json
{
  "success": true
}
```

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| video_id 不在收藏中 | 404 | `{"success":false,"error":"Not in favorites"}` |
| video_id 格式无效 | 400 | `{"success":false,"error":"Invalid video_id"}` |

**示例**:

```bash
curl -X DELETE http://localhost:8080/api/favorites/42
```

---

### A.5 黑名单接口

#### A.5.1 获取黑名单列表

**端点**: `GET /api/blacklist`

**描述**: 获取所有已屏蔽的目录列表。

**Query Parameters**: 无

**成功响应 (200)**:

```json
[
  {
    "id": 1,
    "path": "/videos/unwanted",
    "created_at": 1712345678
  }
]
```

**响应字段定义**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | 黑名单条目 ID |
| `path` | string | 被屏蔽的目录绝对路径 |
| `created_at` | integer | 添加时间戳（Unix 秒） |

**示例**:

```bash
curl http://localhost:8080/api/blacklist
```

---

#### A.5.2 添加黑名单

**端点**: `POST /api/blacklist`

**描述**: 将目录加入黑名单。该目录下的视频标记为黑名单状态（不物理删除），从视频列表中隐藏。

**Request Body (JSON):**

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `path` | string | 是 | 1~512 字符 | 要屏蔽的目录绝对路径 |

**请求示例**:

```json
{
  "path": "/videos/unwanted_directory"
}
```

**成功响应 (200)**:

```json
{
  "success": true,
  "data": {
    "id": 1
  }
}
```

**副作用**:
1. 在 `blacklist` 表中插入新条目
2. 将该目录下所有 `videos` 记录的 `blacklisted` 标记设为 1
3. 下次视频列表查询将自动过滤掉黑名单视频

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| path 为空或缺失 | 400 | `{"success":false,"error":"Invalid path"}` |
| 目录不存在于文件系统 | 400 | `{"success":false,"error":"Directory does not exist"}` |
| 目录已在黑名单中 | 409 | `{"success":false,"error":"Directory already in blacklist"}` |
| path 超过长度限制 | 400 | `{"success":false,"error":"Path too long"}` |

**示例**:

```bash
curl -X POST http://localhost:8080/api/blacklist \
  -H "Content-Type: application/json" \
  -d '{"path":"/videos/unwanted_directory"}'
```

---

#### A.5.3 移除黑名单

**端点**: `DELETE /api/blacklist/:id`

**描述**: 从黑名单中移除目录。该目录下的视频恢复可见，并触发重新扫描。

**Path Parameters:**

| 参数 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `id` | integer | 是 | > 0 | 黑名单条目 ID |

**成功响应 (200)**:

```json
{
  "success": true,
  "message": "Directory removed from blacklist and videos are being restored"
}
```

**副作用**:
1. 从 `blacklist` 表删除条目
2. 将该目录下所有 `videos` 记录的 `blacklisted` 标记恢复为 0
3. 对该目录触发 `video_scanner_scan()` 重新扫描（异步）

**异常场景**:

| 场景 | 状态码 | 响应 |
|------|--------|------|
| 黑名单条目不存在 | 404 | `{"success":false,"error":"Blacklist entry not found"}` |
| id 格式无效 | 400 | `{"success":false,"error":"Invalid id"}` |

**示例**:

```bash
curl -X DELETE http://localhost:8080/api/blacklist/1
```

---

### A.6 静态资源与视频流

#### A.6.1 静态文件服务

**端点**: `GET /{path}`

**描述**: 服务 `web/static/` 目录下的静态文件（HTML/CSS/JS）。

**路由规则**:
- 非 `/api/` 且非 `/video/` 开头的 GET 请求
- 映射到 `web/static/{path}`
- 支持 MIME 类型推断（`.html` → `text/html`, `.css` → `text/css`, `.js` → `application/javascript`）

**安全约束**:
- 禁止路径遍历（`..` 被拒绝）
- 仅限 `web/static/` 目录内文件
- 目录列表禁止（返回 404）

**正常响应 (200)**: 文件内容 + 正确的 Content-Type
**异常响应 (404)**: 文件不存在

---

#### A.6.2 视频文件流式传输

**端点**: `GET /video/{relative_path}`

**描述**: 流式传输视频文件，支持 Range Requests（详见 A.1.5）。

**路径解析**:
- `/video/` 前缀后的部分视为相对于视频根目录（`--video-dir` 参数）的路径
- 例：`/video/movies/test.mp4` → `{video_dir}/movies/test.mp4`

**MIME 类型映射**:

| 扩展名 | Content-Type |
|--------|-------------|
| `.mp4` | `video/mp4` |
| `.webm` | `video/webm` |

**安全约束**:
- 路径规范化（解析 `.` 和 `..`）
- 最终路径必须在 `video_dir` 内
- 仅允许 `.mp4` 和 `.webm` 文件（**扩展名匹配不区分大小写**，如 `.MP4`、`.WebM` 也可访问）
- 拒绝符号链接文件（`lstat()` 检测 `S_ISLNK`）

---

### A.7 API 路由总表

| # | 方法 | 路径 | 处理函数 | 说明 |
|---|------|------|----------|------|
| 1 | GET | `/api/videos` | `api_get_videos()` | 视频列表（支持 search/category/limit/offset） |
| 2 | GET | `/api/videos/random` | `api_get_random()` | 随机视频 |
| 3 | GET | `/api/categories` | `api_get_categories()` | 分类列表 |
| 4 | GET | `/api/history` | `api_get_history()` | 播放历史 |
| 5 | POST | `/api/history` | `api_add_history()` | 添加/更新历史 |
| 6 | DELETE | `/api/history/:id` | `api_delete_history()` | 删除单条历史 |
| 7 | DELETE | `/api/history` | `api_clear_history()` | 清空历史 |
| 8 | GET | `/api/favorites` | `api_get_favorites()` | 收藏列表 |
| 9 | POST | `/api/favorites` | `api_add_favorite()` | 添加收藏 |
| 10 | DELETE | `/api/favorites/:video_id` | `api_remove_favorite()` | 取消收藏 |
| 11 | GET | `/api/blacklist` | `api_get_blacklist()` | 黑名单列表 |
| 12 | POST | `/api/blacklist` | `api_add_blacklist()` | 添加黑名单 |
| 13 | DELETE | `/api/blacklist/:id` | `api_remove_blacklist()` | 移除黑名单 |
| 14 | GET | `/video/*` | (http_server 内部) | 视频流式传输 + Range |
| 15 | GET | `/*` | (http_server 内部) | 静态文件服务 |

---

## B 篇：模块间内部接口（C 函数契约）

### B.1 数据结构定义

#### B.1.1 公共类型 ([local_video.h](src/include/local_video.h))

```c
typedef enum {
    LV_OK = 0,                /* 操作成功 */
    LV_ERROR_INVALID_ARG,     /* 参数无效 */
    LV_ERROR_MEMORY,           /* 内存分配失败 */
    LV_ERROR_IO,               /* IO 错误 */
    LV_ERROR_DB,               /* 数据库错误 */
    LV_ERROR_NETWORK,          /* 网络错误 */
    LV_ERROR_UNKNOWN           /* 未知错误 */
} lv_error_t;

typedef enum {
    LV_LOG_DEBUG,
    LV_LOG_INFO,
    LV_LOG_WARNING,
    LV_LOG_ERROR
} lv_log_level_t;
```

#### B.1.2 视频元数据 ([db_manager.h](src/modules/db_manager/db_manager.h))

```c
typedef struct {
    int64_t id;            /* 主键，自增 */
    char path[512];        /* 文件绝对路径 (UNIQUE) */
    char title[256];       /* 文件名（含扩展名） */
    char category[256];    /* 分类名（父目录名） */
    int64_t size;          /* 文件大小（字节） */
    int64_t created_at;    /* 创建时间（Unix 秒） */
} VideoInfo;
```

**字段约束**:

| 字段 | 约束 | 说明 |
|------|------|------|
| `id` | > 0, 自增 | 由数据库分配 |
| `path` | 1~511 字符, UNIQUE | 绝对路径，不以 `/` 结尾 |
| `title` | 1~255 字符 | 文件名，含扩展名 |
| `category` | 0~255 字符 | 允许空字符串（根目录视频） |
| `size` | ≥ 0 | `stat()` 结果的 `st_size` |
| `created_at` | > 0 | `strftime('%s','now')` |

#### B.1.3 历史记录 ([db_manager.h](src/modules/db_manager/db_manager.h))

```c
typedef struct {
    int64_t id;              /* 主键 */
    int64_t video_id;        /* 外键 → videos.id */
    char video_title[256];   /* 标题快照 */
    char video_path[512];    /* 路径快照 */
    int64_t position;        /* 播放位置（毫秒） */
    int64_t played_at;       /* 播放时间（Unix 秒） */
} HistoryInfo;
```

**设计决策**: `video_title` 和 `video_path` 作为冗余快照存储，确保视频被删除后历史记录仍可显示。

#### B.1.4 收藏记录 ([db_manager.h](src/modules/db_manager/db_manager.h))

```c
typedef struct {
    int64_t id;              /* 主键 */
    int64_t video_id;        /* 外键 → videos.id (UNIQUE) */
    char video_title[256];   /* 标题快照 */
    char video_path[512];    /* 路径快照 */
    int64_t created_at;      /* 收藏时间（Unix 秒） */
} FavoriteInfo;
```

**约束**: `video_id` UNIQUE — 同一视频只能收藏一次。

#### B.1.5 黑名单记录 ([db_manager.h](src/modules/db_manager/db_manager.h))

```c
typedef struct {
    int64_t id;              /* 主键 */
    char path[512];          /* 目录绝对路径 (UNIQUE) */
    int64_t created_at;      /* 添加时间（Unix 秒） */
} BlacklistInfo;
```

**约束**: `path` UNIQUE — 同一目录只能黑名单一次。

#### B.1.6 HTTP 请求结构体 ([http_request.h](src/modules/http_server/http_request.h))

```c
#define HTTP_METHOD_MAX 16
#define HTTP_PATH_MAX 512
#define HTTP_VERSION_MAX 16
#define HTTP_HOST_MAX 256
#define HTTP_CONN_MAX 16
#define HTTP_CONTENT_TYPE_MAX 64

typedef struct {
    char method[HTTP_METHOD_MAX];       /* "GET", "POST", "DELETE" */
    char path[HTTP_PATH_MAX];            /* 请求路径 (含 query string 前的部分) */
    char query[HTTP_PATH_MAX];           /* 查询字符串 (原始，不含 ?) */
    char version[HTTP_VERSION_MAX];      /* "HTTP/1.1" */
    char host[HTTP_HOST_MAX];            /* Host 头部值 */
    char connection[HTTP_CONN_MAX];      /* Connection 头部值 */
    int64_t range_start;                 /* Range 请求起始字节 (-1 表示无) */
    int64_t range_end;                   /* Range 请求结束字节 (-1 表示到末尾) */
    char if_modified_since[64];          /* If-Modified-Since 头部值 */
} HttpRequest;
```

**调用约定**:
- 由 `http_request_parse(client_fd, &req)` 填充
- `path` 包含完整的 URL 路径（如 `/api/videos?search=test` 中 path 为 `/api/videos`，query 为 `search=test`）
- `range_start == -1` 表示没有 Range 请求头

#### B.1.7 HTTP 响应结构体 ([http_response.h](src/modules/http_server/http_response.h))

```c
typedef struct {
    int status_code;                     /* HTTP 状态码 (200, 206, 404...) */
    const char *reason;                  /* 原因短语 ("OK", "Not Found") */
    char content_type[HTTP_CONTENT_TYPE_MAX]; /* MIME 类型 */
    int64_t content_length;              /* 内容长度（字节） */
    bool keep_alive;                     /* 是否保持连接（当前固定 false） */
} HttpResponse;
```

**初始化**: 必须先调用 `http_response_init(&resp)` 进行零初始化。

---

### B.2 回调接口契约

#### B.2.1 视频遍历回调

```c
typedef int (*video_callback_t)(const VideoInfo *video, void *user_data);
```

**契约**:
- **调用方**: db_manager（在 SQL 查询结果遍历时调用）
- **实现方**: api_handler（构建 JSON 输出）
- **参数**:
  - `video`: 当前行的 VideoInfo 结构体指针（只读，生命周期仅在回调期间有效）
  - `user_data`: 调用方传入的上下文指针（api_handler 中是 response_buffer_t*）
- **返回值**: `0` 继续遍历，非 0 中止遍历
- **线程安全**: 回调在调用 db_manager 函数的同一线程中执行

**使用模式**:

```c
// 调用方 (api_handler)
static int my_callback(const VideoInfo *v, void *ud) {
    response_buffer_t *buf = (response_buffer_t *)ud;
    // 构建 JSON ...
    return 0; // 继续下一条
}

db_manager_video_get_all(my_callback, &buf);
```

#### B.2.2 其他回调类型

| 回调类型 | 数据结构 | 用途 |
|----------|----------|------|
| `history_callback_t` | `HistoryInfo *` | 遍历历史记录 |
| `favorite_callback_t` | `FavoriteInfo *` | 遍历收藏列表 |
| `category_callback_t` | `const char *` | 遍历分类名字符串 |
| `blacklist_callback_t` | `BlacklistInfo *` | 遍历黑名单 |

所有回调共享相同的契约模式：返回 `0` 继续，非 0 中止。

---

### B.3 模块对外接口清单

#### B.3.1 config 模块 ([config.h](src/modules/config/config.h))

```c
void config_parse_args(int argc, char *argv[]);
```

**功能**: 解析命令行参数并存储全局配置。
**支持参数**:
- `--port <number>`: HTTP 监听端口（默认 8080）
- `--video-dir <path>`: 视频根目录（必填）
- `--db-path <path>`: SQLite3 数据库文件路径（默认 `./local_video.db`）
- `--web-root <path>`: 前端静态文件目录（默认 `./web/static`）

**全局配置获取**（通过 config 模块内部 getter，待实现时确定具体签名）:
- `config_get_port() → uint16_t`
- `config_get_video_dir() → const char*`
- `config_get_db_path() → const char*`
- `config_get_web_root() → const char*`

**调用时机**: `main()` 中最早调用，在任何模块 init 之前。

---

#### B.3.2 db_manager 模块 ([db_manager.h](src/modules/db_manager/db_manager.h))

**生命周期**:

```c
lv_error_t db_manager_init(const char *db_path);
// 功能: 打开/创建 SQLite3 数据库，创建表和索引
// 参数: db_path — 数据库文件路径
// 成功: LV_OK
// 失败: LV_ERROR_IO (无法打开), LV_ERROR_DB (DDL 执行失败)

lv_error_t db_manager_close(void);
// 功能: 关闭数据库连接，释放资源
// 应在程序退出前调用
```

**视频操作**:

```c
lv_error_t db_manager_video_insert(const char *path, const char *title,
                                   const char *category, int64_t size);
// 插入新视频记录。path 冲突时返回 LV_ERROR_DB (UNIQUE 约束)

lv_error_t db_manager_video_get_all(video_callback_t cb, void *user_data);
// 获取所有非黑名单视频，按 title 升序

lv_error_t db_manager_video_search(const char *query, video_callback_t cb, void *ud);
// 按 title LIKE '%query%' 模糊搜索（仅搜索非黑名单视频）

lv_error_t db_manager_video_get_by_category(const char *category, video_callback_t cb, void *ud);
// 按分类精确匹配筛选

lv_error_t db_manager_video_get_by_id(int64_t id, VideoInfo *out);
// 按 ID 单条查询，结果写入 out

lv_error_t db_manager_video_get_random(int count, video_callback_t cb, void *ud);
// 随机获取 count 条非黑名单视频

lv_error_t db_manager_video_count(int64_t *out_count);
// 获取非黑名单视频总数

lv_error_t db_manager_video_delete(int64_t id);
// 按 ID 删除视频记录

lv_error_t db_manager_video_update(const char *path, const char *title,
                                   const char *category, int64_t size);
// 按 path 更新视频元数据（UPSERT 语义）

lv_error_t db_manager_video_delete_by_path(const char *path);
// 按路径删除单条视频记录

lv_error_t db_manager_video_delete_by_path_prefix(const char *prefix);
// 按路径前缀批量删除（删除某目录下所有视频）

lv_error_t db_manager_video_blacklist_by_path_prefix(const char *prefix);
// 将某目录下所有视频标记为 blacklisted=1

lv_error_t db_manager_video_unblacklist_all(void);
// 将所有视频的 blacklisted 重置为 0

lv_error_t db_manager_video_unblacklist_by_path_prefix(const char *prefix);
// 将某目录下视频的 blacklisted 重置为 0
```

**历史记录操作**:

```c
lv_error_t db_manager_history_add(int64_t video_id, int64_t position);
// UPSERT: 存在则更新 position + played_at，不存在则插入新记录

lv_error_t db_manager_history_get(history_callback_t cb, void *user_data);
// 获取所有历史记录，按 played_at DESC 排序

lv_error_t db_manager_history_delete(int64_t id);
// 删除单条历史记录

lv_error_t db_manager_history_clear(void);
// 清空所有历史记录
```

**收藏操作**:

```c
lv_error_t db_manager_favorite_add(int64_t video_id);
// 添加收藏。video_id 已存在于 favorites 时返回 LV_ERROR_DB (UNIQUE)

lv_error_t db_manager_favorite_remove(int64_t video_id);
// 取消收藏。video_id 不存在时静默成功（幂等）

lv_error_t db_manager_favorites_list(favorite_callback_t cb, void *user_data);
// 获取所有收藏，按 created_at DESC 排序

lv_error_t db_manager_favorite_check(int64_t video_id, bool *out);
// 检查是否已收藏
```

**分类操作**:

```c
lv_error_t db_manager_category_get_all(category_callback_t cb, void *user_data);
// 获取所有有视频的分类（DISTINCT category），按 name ASC 排序
```

**黑名单操作**:

```c
lv_error_t db_manager_blacklist_add(const char *path, int64_t *out_id);
// 添加黑名单目录。path 已存在时返回 LV_ERROR_DB (UNIQUE)
// out_id 输出新创建的 ID

lv_error_t db_manager_blacklist_delete(int64_t id);
// 删除黑名单条目

lv_error_t db_manager_blacklist_get_all(blacklist_callback_t cb, void *user_data);
// 获取所有黑名单，按 created_at ASC 排序

lv_error_t db_manager_blacklist_check(const char *path, bool *out);
// 检查目录是否已在黑名单

lv_error_t db_manager_blacklist_get_by_id(int64_t id, char *out_path, size_t path_size);
// 按 ID 查询黑名单路径，写入 out_path
```

**线程安全**: 所有函数内部使用互斥锁保护 SQLite3 连接，可从任意线程安全调用。

---

#### B.3.3 http_server 模块 ([http_server.h](src/modules/http_server/http_server.h))

```c
lv_error_t http_server_init(uint16_t port, const char *web_root);
// 创建 socket, bind, 设置选项
// port: 监听端口
// web_root: 静态文件根目录路径
// 失败: LV_ERROR_NETWORK (socket/bind 失败)

lv_error_t http_server_start(void);
// 进入 accept 循环（阻塞调用）
// 每个 accept 创建新 pthread 处理连接
// 内部根据路径分发: /api/* → api_handler, /video/* → 流传输, 其它 → 静态文件

lv_error_t http_server_stop(void);
// 设置停止标志，使 accept 循环退出
// 安全: 可从信号处理函数中调用（仅设置 volatile flag）

lv_error_t http_server_close(void);
// 关闭 socket fd，释放资源
// 应在 stop 之后调用
```

**内部请求处理流水线** (http_server.c):

```
accept() → pthread_create()
  └─ 工作线程:
       ├─ http_request_parse(fd, &req)
       ├─ 路径判断:
       │    ├─ starts_with("/api/") → api_handler_handle(fd, &req)
       │    ├─ starts_with("/video/") → serve_video_file(fd, &req)
       │    └─ 其它 → serve_static_file(fd, &req)
       └─ close(fd)
```

---

#### B.3.4 api_handler 模块 ([api_handler.h](src/modules/api_handler/api_handler.h))

```c
lv_error_t api_handler_handle(int client_fd, const HttpRequest *req);
```

**功能**: REST API 路由分发入口。

**参数**:
- `client_fd`: 已接受连接的 socket 文件描述符
- `req`: 已解析的 HTTP 请求数据

**内部逻辑**:
1. 从 `req->path` 提取 `/api/` 后的路由路径
2. 根据 `req->method` + 路由路径 匹配路由表（见 A.7 总表）
3. 对于 POST 方法：从 socket 读取 request body
4. 调用对应的 `api_xxx()` 处理函数
5. 构建并发送 JSON 响应
6. **不关闭** client_fd（由调用方 http_server 负责）

**返回值**: `LV_OK` 表示已发送响应；出错时也已发送错误响应。

**JSON 构建**: 当前使用手写 buffer_append 方式拼接 JSON（见 api_handler.c 实现），未来可替换为 json_writer_t 流式 API。

---

#### B.3.5 video_scanner 模块 ([video_scanner.h](src/modules/video_scanner/video_scanner.h))

```c
lv_error_t video_scanner_scan(const char *directory);
// 全量扫描: 递归遍历 directory，识别 .mp4/.webm 文件
// 对每个文件: db_manager_video_insert() (忽略重复)
// 同步阻塞调用，扫描大目录可能耗时较长

lv_error_t video_scanner_rescan(void);
// 增量重扫: 先清空 videos 表，再全量扫描 video_dir
// 用于数据库损坏恢复等场景

lv_error_t video_scanner_start_watcher(void);
// 启动 inotify + epoll 文件监控线程
// 监控事件: IN_CREATE, IN_DELETE, IN_MODIFY, IN_MOVED_FROM, IN_MOVED_TO
// 新文件 → insert, 删除文件 → delete_by_path, 移动 → delete+insert
// 异步: 立即返回，后台线程运行

lv_error_t video_scanner_stop_watcher(void);
// 停止监控线程，关闭 inotify fd 和 epoll fd
// 阻塞等待监控线程退出
```

**视频文件识别规则**:
- 文件扩展名（不区分大小写）：`.mp4`, `.webm`
- 普通文件（排除目录、符号链接、设备文件）
- `title` = 文件名（含扩展名）
- `category` = 相对于 video_dir 的父目录路径（用 `/` 分隔）
- 例: `/videos/movies/action/Test.mp4` → category=`movies/action`, title=`Test.mp4`

---

#### B.3.6 notifier 模块 ([notifier.h](src/shared/notifier/notifier.h))

```c
void notifier_chain_init(notifier_chain_t *chain);
// 初始化空链表

void notifier_block_init(notifier_block_t *nb, notifier_func_t func, int priority);
// 初始化通知块: func 为回调函数, priority 为优先级（数值越小越先执行）

lv_error_t notifier_chain_register(notifier_chain_t *chain, notifier_block_t *nb);
// 注册通知块到链表（按 priority 升序插入）

lv_error_t notifier_chain_unregister(notifier_chain_t *chain, notifier_block_t *nb);
// 从链表中移除通知块

notify_result_t notifier_chain_notify(notifier_chain_t *chain, void *data);
// 遍历链表，依次调用每个注册的 func(data)
// 返回 NOTIFY_STOP 时中断后续通知
```

**数据结构**:

```c
typedef notify_result_t (*notifier_func_t)(void *data);

typedef struct notifier_block {
    notifier_func_t func;     // 回调函数
    int priority;             // 优先级
    list_node_t node;         // 链表节点（侵入式）
} notifier_block_t;

struct notifier_chain {
    list_node_t head;         // 链表头
};
```

---

### B.4 json_writer 流式接口 ([json.h](src/shared/json/json.h))

```c
typedef struct {
    FILE *file;    // 输出目标（可以是 fopen 的文件或 fdopen 的 socket）
    int first;     // 对象/数组首个元素标记
    int in_array;  // 是否在数组上下文中
} json_writer_t;

lv_error_t json_writer_init(json_writer_t *writer, FILE *file);
// 初始化 writer，绑定输出文件

lv_error_t json_start_object(json_writer_t *writer);   // 写入 "{"
lv_error_t json_end_object(json_writer_t *writer);     // 写入 "}"
lv_error_t json_start_array(json_writer_t *writer);    // 写入 "["
lv_error_t json_end_array(json_writer_t *writer);      // 写入 "]"

lv_error_t json_add_key(json_writer_t *writer, const char *key);   // 写入 "key":
lv_error_t json_add_string(json_writer_t *writer, const char *value);  // 写入 "value"
lv_error_t json_add_int(json_writer_t *writer, int value);           // 写入数字
lv_error_t json_add_bool(json_writer_t *writer, bool value);         // 写入 true/false
lv_error_t json_add_null(json_writer_t *writer);                     // 写入 null
```

**使用模式**:

```c
json_writer_init(&w, stdout);
json_start_object(&w);
  json_add_key(&w, "success"); json_add_bool(&w, true);
  json_add_key(&w, "data");
  json_start_array(&w);
    json_start_object(&w);
      json_add_key(&w, "id"); json_add_int(&w, 42);
    json_end_object(&w);
  json_end_array(&w);
json_end_object(&w);
// 输出: {"success":true,"data":[{"id":42}]}
```

---

## C 篇：事件通知接口（Notifier Chain）

### C.1 事件类型定义

```c
typedef enum {
    /* ===== 视频相关事件 ===== */
    EVENT_VIDEO_SCANNED,             /* 全量扫描完成 */
    EVENT_VIDEO_ADDED,               /* 新视频被发现（inotify/create） */
    EVENT_VIDEO_REMOVED,             /* 视频被删除（inotify/delete） */
    EVENT_VIDEO_CHANGED,             /* 视频元数据变更（修改/移动） */

    /* ===== 播放相关事件 ===== */
    EVENT_PLAYBACK_STARTED,          /* 用户开始播放某个视频 */
    EVENT_PLAYBACK_STOPPED,          /* 用户停止/结束播放 */
    EVENT_PLAYBACK_POSITION_UPDATE,  /* 播放位置更新（定期保存） */

    /* ===== 收藏相关事件 ===== */
    EVENT_FAVORITE_ADDED,            /* 视频被添加到收藏 */
    EVENT_FAVORITE_REMOVED,          /* 视频从收藏移除 */

    /* ===== 黑名单相关事件 ===== */
    EVENT_BLACKLIST_ADDED,           /* 目录加入黑名单 */
    EVENT_BLACKLIST_REMOVED,         /* 目录从黑名单移除 */

    EVENT_COUNT                      /* 事件总数（哨兵值） */
} event_type_t;
```

### C.2 事件数据载荷 (Event Payload)

每种事件的 `void *data` 携带不同数据：

| 事件 | data 类型 | 说明 |
|------|-----------|------|
| `EVENT_VIDEO_SCANNED` | `int64_t*` | 扫描到的视频数量 |
| `EVENT_VIDEO_ADDED` | `VideoInfo*` | 新增的视频信息 |
| `EVENT_VIDEO_REMOVED` | `const char*` | 被删除的视频路径 |
| `EVENT_VIDEO_CHANGED` | `VideoInfo*` | 变更后的视频信息 |
| `EVENT_PLAYBACK_STARTED` | `int64_t*` | video_id |
| `EVENT_PLAYBACK_STOPPED` | `int64_t*` | video_id |
| `EVENT_PLAYBACK_POSITION_UPDATE` | `int64_t[2]` | [video_id, position] |
| `EVENT_FAVORITE_ADDED` | `int64_t*` | video_id |
| `EVENT_FAVORITE_REMOVED` | `int64_t*` | video_id |
| `EVENT_BLACKLIST_ADDED` | `const char*` | 黑名单目录路径 |
| `EVENT_BLACKLIST_REMOVED` | `const char*` | 黑名单目录路径 |

### C.3 事件订阅矩阵

| 事件 | 发布者 | 订阅者 | 优先级 | 动作 |
|------|--------|--------|--------|------|
| `EVENT_VIDEO_SCANNED` | video_scanner | api_handler | 10 | 标记视频列表缓存失效 |
| `EVENT_VIDEO_ADDED` | video_scanner | api_handler | 10 | 标记缓存失效 |
| `EVENT_VIDEO_REMOVED` | video_scanner | api_handler | 10 | 标记缓存失效 |
| `EVENT_VIDEO_CHANGED` | video_scanner | api_handler | 10 | 标记缓存失效 |
| `EVENT_BLACKLIST_ADDED` | api_handler | (内部) | 0 | 已在同函数内处理 |
| `EVENT_BLACKLIST_REMOVED` | api_handler | (内部) | 0 | 已在同函数内处理 |

> 当前阶段通知链主要用于视频变更事件驱动的缓存失效机制。播放和收藏事件预留接口，供未来扩展使用。

### C.4 通知链注册模式

```c
/* 1. 定义全局链表和通知块 */
static notifier_chain_t video_event_chain;
static notifier_block_t on_video_changed_nb;

/* 2. 在 MODULE_SUB 阶段注册 */
static void my_module_sub(void) {
    notifier_chain_init(&video_event_chain);
    notifier_block_init(&on_video_changed_nb, my_on_video_changed_handler, 10);
    notifier_chain_register(&video_event_chain, &on_video_changed_nb);
}
MODULE_SUB(my_module_sub, "my_module");

/* 3. 实现处理函数 */
static notify_result_t my_on_video_changed_handler(void *data) {
    event_type_t event = *(event_type_t *)data;
    if (event == EVENT_VIDEO_SCANNED || event == EVENT_VIDEO_ADDED ||
        event == EVENT_VIDEO_REMOVED || event == EVENT_VIDEO_CHANGED) {
        cache_invalidated = true;
    }
    return NOTIFY_OK;
}

/* 4. 发布者触发通知 */
notifier_chain_notify(&video_event_chain, &event);
```

---

## D 篇：错误码体系

### D.1 层级化错误码

```
┌─────────────────────────────────────────────┐
│              D.1 应用层错误码                  │
│  (REST API 响应中的业务错误描述)              │
├─────────────────────────────────────────────┤
│              D.2 函数层错误码                  │
│  (lv_error_t 返回值)                         │
├─────────────────────────────────────────────┤
│              D.3 HTTP 状态码                   │
│  (HTTP Response status line)                │
└─────────────────────────────────────────────┘
```

### D.2 lv_error_t 返回码与 HTTP 状态码映射

| lv_error_t | 含义 | 对应 HTTP 状态码 | API 错误消息示例 |
|------------|------|-----------------|-----------------|
| `LV_OK` | 成功 | 200 | - |
| `LV_ERROR_INVALID_ARG` | 参数无效 | 400 | `"Invalid parameter"` |
| `LV_ERROR_MEMORY` | 内存不足 | 500 | `"Internal error"` |
| `LV_ERROR_IO` | IO 错误 | 500 | `"Internal error"` |
| `LV_ERROR_DB` | 数据库错误 | 视情况 | 400/409/500 |
| `LV_ERROR_NETWORK` | 网络错误 | 500 | `"Internal error"` |
| `LV_ERROR_UNKNOWN` | 未知错误 | 500 | `"Internal error"` |

### D.3 业务错误消息汇总

| 错误消息 | HTTP 状态码 | 触发条件 |
|---------|-------------|----------|
| `"Missing required fields"` | 400 | POST body 缺少必填字段 |
| `"Invalid JSON body"` | 400 | POST body 无法解析为 JSON |
| `"Invalid path"` | 400 | 黑名单路径为空或格式无效 |
| `"Directory does not exist"` | 400 | 黑名单目录不在文件系统中 |
| `"Directory already in blacklist"` | 409 | 黑名单路径 UNIQUE 冲突 |
| `"Already in favorites"` | 409 | 收藏 video_id UNIQUE 冲突 |
| `"Video not found"` | 400 | 引用的 video_id 在 DB 中不存在 |
| `"Not found"` | 404 | 通用资源未找到 |
| `"Blacklist entry not found"` | 404 | 黑名单 ID 不存在 |
| `"Not in favorites"` | 404 | 取消收藏的 video_id 未收藏 |
| `"Invalid video_id"` | 400 | 路径参数中的 ID 格式无效 |
| `"Path too long"` | 400 | 超过 512 字符限制 |
| `"Internal error"` | 500 | 服务器内部未预期错误 |

### D.4 错误处理最佳实践

**api_handler 层（REST API 入口）**:
1. 捕获所有 `lv_error_t` 返回值
2. 将其转换为对应 HTTP 状态码 + 业务错误消息
3. **绝不**将内部错误详情（如 SQL 错误文本、文件路径）暴露给客户端
4. 日志中记录详细错误信息（`log_error()`）

**db_manager 层（数据访问）**:
1. 所有 SQLite3 操作检查返回值
2. 使用预处理语句防止 SQL 注入
3. UNIQUE 约束冲突返回 `LV_ERROR_DB`（由上层决定映射为 400 还是 409）

**http_server 层（网络）**:
1. `send()/write()` 检查返回值和 errno
2. 客户端断开（EPIPE/ECONNRESET）不视为致命错误，仅 log 后清理
3. SIGPIPE 信号在 main() 中忽略

---

## E 篇：前端调用参考

### E.1 JavaScript 封装示例

```javascript
const API_BASE = 'http://localhost:8080/api';

async function apiGet(path) {
    const res = await fetch(`${API_BASE}${path}`);
    return await res.json();
}

async function apiPost(path, body) {
    const res = await fetch(`${API_BASE}${path}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
    return await res.json();
}

async function apiDelete(path) {
    const res = await fetch(`${API_BASE}${path}`, {
        method: 'DELETE'
    });
    return await res.json();
}

/* 视频 */
export const videos = {
    list: (search, category) => {
        const params = new URLSearchParams();
        if (search) params.set('search', search);
        if (category) params.set('category', category);
        const qs = params.toString();
        return apiGet(`/videos${qs ? '?' + qs : ''}`);
    },
    random: () => apiGet('/videos/random'),
};

/* 历史 */
export const history = {
    list: () => apiGet('/history'),
    add: (videoId, position) => apiPost('/history', { video_id: videoId, position }),
    clear: () => apiDelete('/history'),
};

/* 收藏 */
export const favorites = {
    list: () => apiGet('/favorites'),
    add: (videoId) => apiPost('/favorites', { video_id: videoId }),
    remove: (videoId) => apiDelete(`/favorites/${videoId}`),
};

/* 黑名单 */
export const blacklist = {
    list: () => apiGet('/blacklist'),
    add: (path) => apiPost('/blacklist', { path }),
    remove: (id) => apiDelete(`/blacklist/${id}`),
};

/* 分类 */
export const categories = {
    list: () => apiGet('/categories'),
};
```

### E.2 视频播放器集成

```html
<video id="player" controls preload="metadata">
</video>

<script>
const player = document.getElementById('player');
let currentVideoId = null;
let lastSavedPosition = 0;

function playVideo(video) {
    currentVideoId = video.id;

    player.src = '/video' + encodeURIComponent(video.path.slice(config.videoDir.length));
    player.load();

    history.list().then(records => {
        const record = records.find(r => r.video_id === video.id);
        if (record && record.position > 0) {
            if (confirm(`继续上次播放? (上次位置: ${formatTime(record.position)})`)) {
                player.currentTime = record.position / 1000;
            }
        }
    });

    player.play();
}

player.addEventListener('timeupdate', () => {
    const pos = Math.floor(player.currentTime * 1000);
    if (Math.abs(pos - lastSavedPosition) > 5000) {
        history.add(currentVideoId, pos).catch(() => {});
        lastSavedPosition = pos;
    }
});

player.addEventListener('ended', () => {
    history.add(currentVideoId, Math.floor(player.currentTime * 1000)).catch(() => {});
});

function formatTime(ms) {
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const sec = s % 60;
    return `${m}:${sec.toString().padStart(2, '0')}`;
}
</script>
```

---

## 附录：接口版本与变更日志

### v1.0 (当前)

- 初始版本，覆盖 12 个 REST API 端点
- 定义 6 个模块的 C 语言内部接口
- 定义 11 种通知链事件类型
- 定义层级化错误码体系
