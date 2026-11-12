# Reactor User Service

基于 C++17、Reactor 模型和 Protobuf 的用户管理后台服务。项目目标是作为 Linux C++ 服务端方向的简历项目：能讲清网络模型、协议编解码、业务分层、数据库连接池、线程池和基础排障。

## 功能

### 网络与运行时

- `epoll` + ET 的主从 Reactor：主 loop 只负责 accept，新连接轮询分发给 sub loop（one loop per thread），`io_threads=0` 时退化为单 Reactor。
- 自定义二进制帧头：`body_length + message_type + request_id`，处理 TCP 粘包和半包；帧长非法/超限时返回错误码并主动断开，不会让异常穿透打挂进程。
- `PING/PONG` 心跳协议：客户端 30 秒一次保活，服务端 5 分钟空闲自动回收连接。
- 输出缓冲高水位保护：慢客户端堆积超过 4MB 直接断开，防止内存被吃光。
- 优雅关闭：SIGTERM/SIGINT 后停止 accept、drain 输出缓冲、join 线程池再退出。
- 业务线程池处理 Protobuf 解析、密码哈希和 DB 访问，不阻塞事件循环；跨线程写通过 eventfd 唤醒所属 loop。

### 业务与安全

- 用户注册、登录、用户查询、权限校验、删除用户；管理员可新增用户、冻结/解冻账号、删除离职人员账号（单条删除 + 二次确认，不能删除自己）。
- 普通用户自助能力（WHOAMI / MY_SESSIONS 协议）：查询本人实时账号信息、查看本人全部在线设备、把其他设备踢下线、分页查询本人最近操作并可导出 CSV（服务端强制按登录身份过滤，请求里的 actor 声明不可信）。
- **超级管理员唯一**：只有首次初始化时创建的账号是 admin；之后无论是自助注册还是管理员在后台新增账号，角色一律为普通 user（服务端二次校验，请求里声明 admin 会被拒绝）。
- 修改密码必须校验旧密码（防会话劫持后直接改密）；管理员重置密码改为生成 12 位随机口令、仅回传一次。
- 密码存储使用 PBKDF2-HMAC-SHA256（自实现，12000 轮迭代 + 16 字节随机盐，本机约 20~40ms，适合演示级强度；如需生产强度可调高 `PasswordHasher::kDefaultIterations`，存储格式自带轮数字段，旧哈希轮数不足时登录会自动重哈希升级），登录时自动识别旧的 `std::hash` 哈希并原地升级，无需停机迁移。
- 登录失败限流：按用户名 + 来源 IP 做 5 分钟滑动窗口计数，连续失败 5 次锁定 5 分钟，响应携带剩余尝试次数。
- 用户列表支持服务端真实分页、关键字模糊搜索、状态过滤和排序（LIMIT/OFFSET + username 索引），返回 total 供前端渲染分页器。
- 审计日志：注册、登录、改密、重置、冻结、删除、强制下线等敏感操作全部留痕（actor/action/target/result/IP/时间），管理员可分页查询。
- 统一错误码（UNAUTHENTICATED/PERMISSION_DENIED/TOO_MANY_ATTEMPTS 等），客户端可本地化提示。
- 管理员可查看在线会话（含来源地址、登录时间、最近活跃时间）并强制下线；普通用户仅能查看并踢下线自己的会话（当前连接不可踢，返回 CONFLICT）。
- STATS 运行指标协议：管理员可实时查询服务端运行时长、当前/累计连接数、IO 与业务线程数、存储后端、会话数（连接数用原子计数跨线程读取，避免锁竞争）。

### 客户端与测试

- Qt Widgets 桌面工作台：登录、数据总览首页（账号/正常/冻结/在线四张统计卡片 + 最近动态）、用户管理（分页/搜索/过滤、新增用户、重置密码、冻结/解冻、单条删除、批量冻结解冻、导出 CSV）、在线会话管理、审计日志查询、服务端监控页、我的空间（普通用户默认页 & 管理员点头像进入：WHOAMI 账号信息卡片、改密表单、“我的登录设备”、“我的最近操作”）、心跳保活、重置密码一次性弹窗展示。
- 普通用户功能：我的空间（账号 ID/状态/登录 IP/登录时间/最近活跃/上次改密时间）、修改自己的密码、查看自己的最近操作并可导出 CSV（审计按 actor 强制限定为本人）、我的登录设备（只含本人会话，可把其他设备踢下线）。
- 登录失败按错误码给出中文原因；重置密码弹窗展示一次性随机口令；总览/在线/审计/监控页仅管理员可见。单条删除账号需二次确认，且不能删除当前登录账号。
- Windows 控制台 Demo 支持本地文件持久化；完整 Linux 服务端支持 MySQL 连接池、users/audit_logs 双表持久化。
- CTest 单元测试：编解码、线程池、密码哈希、登录限流、分页查询、审计、协议操作、Qt 客户端组件，服务端 13 项 + Qt 客户端 5 项，共 18 项。
- 命令行测试客户端覆盖全量协议交互，含 PING、审计日志和一次性重置密码。

## 目录

```text
.
├── CMakeLists.txt
├── proto/user.proto
├── src
│   ├── base
│   ├── business
│   ├── db
│   ├── net
│   └── protocol
├── client/test_client.cpp
├── client/QtUserClient
│   ├── QtTcpClient.*
│   ├── ProtoMessages.*
│   ├── LoginWidget.*
│   ├── DashboardWidget.*
│   └── MainWindow.*
├── tests
└── config
```

## 依赖

Linux/WSL 推荐环境：

- CMake 3.20+
- C++17 编译器
- Protobuf 编译器和 C++ 开发库
- MySQL 或 MariaDB client 开发库

Windows 桌面客户端额外需要：

- Qt 6 Widgets 和 Network 模块。
- CMake 和 Ninja。
- 本项目的 Qt 客户端内置轻量 Protobuf wire adapter，因此不要求 Windows 安装 Protobuf；服务端仍使用标准 `protoc` 生成代码。

Ubuntu/WSL 示例：

```bash
sudo apt update
sudo apt install -y build-essential cmake protobuf-compiler libprotobuf-dev default-libmysqlclient-dev mysql-server
```

## 数据库初始化

```bash
mysql -u root -p < config/user.sql
```

修改 `config/server.conf` 里的 MySQL 用户名、密码和端口。

## 构建

当前 Windows 环境可以先跑不依赖 Protobuf/MySQL/Linux epoll 的核心测试：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_core_windows.ps1
```

这个模式等价于：

```powershell
cmake -S . -B build-core -G Ninja -DENABLE_PROTOBUF=OFF -DENABLE_MYSQL=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

直接运行一个控制台演示程序：

```powershell
.\build-core\console_demo.exe
```

这个 Demo 使用本地文件仓储，适合快速展示注册、登录、查询、权限校验、修改密码、删除用户和用户列表查询；完整服务端仍然建议在 Linux/WSL 中运行。

控制台 Demo 的用户数据会保存到：

```text
data/users.txt
```

控制台 Demo 支持：

- 注册用户
- 登录
- 查询用户
- 权限校验
- 修改密码
- 删除用户
- 用户列表查询

构建 Qt 桌面客户端：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_qt_client_windows.ps1
```

如果 Qt 安装路径不同，可以显式指定：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_qt_client_windows.ps1 `
  -QtPrefixPath "D:\Qt\6.11.0\mingw_64"
```

脚本默认输出到项目内的 `build-qt` 目录；如果项目路径包含中文导致 Qt AutoMOC 生成目录报错，可以用 `-BuildDir` 指定一个纯英文路径。

完整服务端建议在 Linux/WSL 里构建：

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

如果是刚初始化的 Ubuntu/WSL，可以先运行：

```bash
bash scripts/setup_wsl_ubuntu.sh
```

## 运行

### 业务流程

1. 首次启动且用户表为空时，登录页的“创建管理员账号”走一次免鉴权初始化注册；初始化完成后必须登录管理员账号。
2. 登录成功后，服务端把用户身份绑定到当前 TCP 连接，会话不会相信客户端后续请求中伪造的用户名。
3. 管理员进入用户管理页，可新增用户、按关键字/状态搜索、翻页浏览列表、重置密码、冻结/解冻或删除其他用户；管理员不能冻结或删除自己的当前账号。
4. 普通用户登录后不具备用户列表、在线用户和审计日志权限，默认进入「我的空间」：可查看账号信息（WHOAMI，含登录 IP、登录时间、最近活跃、上次改密时间）、修改自己的密码（需验证旧密码）、查看并导出自己的最近操作（服务端强制限定审计 actor 为本人）、在「我的登录设备」中查看并踢下线自己的其他会话（不能踢当前连接），或退出账号。
5. 服务端每次登录都会检查账号状态；状态为冻结时直接拒绝登录。连续登录失败会被限流锁定，响应协议携带剩余尝试次数（客户端当前不展示，仅按错误码给出中文原因）。
6. 修改密码必须提供旧密码；管理员重置密码后，新随机口令只在客户端弹窗显示一次，请立即转达并妥善保存。
7. 客户端每 30 秒发送一次 PING 保活；服务端 5 分钟无数据的空闲连接会被自动回收。

启动服务端：

```bash
./build/reactor_user_server config/server.conf
```

另开终端运行客户端：

```bash
./build/test_client 127.0.0.1 9000
```

客户端会依次发送注册、登录、查询用户、权限校验请求。

命令行客户端现在还会继续演示用户列表、修改密码和删除用户。

启动 Windows 桌面客户端：

```powershell
$env:PATH = "E:\QT\6.11.0\mingw_64\bin;$env:PATH"
.\build-qt\client\QtUserClient\QtUserClient.exe
```

也可以直接双击项目根目录的 `login.bat` 启动客户端并手动输入账号密码；传参 `login.bat 用户名 密码` 可自动登录。

桌面客户端登录页只需要输入账号和密码，服务地址固定为本机 `127.0.0.1:9000`。开发环境初始化脚本会创建 MySQL 数据库、账号并写入一个管理员：

```text
账号：admin
密码：123456
```

启用 MySQL 的 Linux 构建使用 `config/server.conf` 中的数据库配置；首次准备开发环境时执行 `config/dev_mysql_seed.sql`，再启动 `build/reactor_user_server`。

如果已经在 Linux/WSL 中完成构建，也可以一键初始化数据库、启动服务端并运行客户端：

```bash
bash scripts/run_linux_integration.sh
```

## 模块说明

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 基础设施 | `src/base` | 配置读取、日志、线程池、SHA-256/PBKDF2 加密原语 |
| 网络层 | `src/net` | 主从 Reactor（`EventLoop` / `EventLoopThreadPool`）、TCP 连接、定时器队列、信号处理、服务端监听 |
| 协议层 | `src/protocol`、`proto` | 二进制帧编解码和 Protobuf 消息定义（含 PING/PONG、审计、WHOAMI、我的会话列表、错误码） |
| 桌面客户端 | `client/QtUserClient` | Qt Widgets 工作台、TCP 客户端、请求会话和操作反馈 |
| 业务层 | `src/business` | 注册/登录/改密/删除/冻结、分页查询、登录限流、审计服务、密码哈希 |
| 文件仓储 | `src/business/File*Repository.*` | Windows 控制台 Demo 的本地文件持久化（用户 + 审计） |
| 数据层 | `src/db` | MySQL 连接、连接池、用户/审计仓储实现 |
| 测试 | `tests` | 协议、线程池、密码哈希、限流、分页、审计、用户服务单元测试 |

## 关键配置（config/server.conf）

| 配置 | 说明 |
| --- | --- |
| `io_threads` | sub Reactor 数量，0 表示退化为单 Reactor（可做对比压测） |
| `worker_threads` | 业务线程池大小 |
| `idle_timeout_seconds` | 连接空闲回收阈值（0 关闭） |
| `login_max_failures / login_window_seconds / login_lock_seconds` | 登录失败限流参数 |
| `allow_self_register` | 是否允许未登录用户自助注册（默认 1；注册出的账号角色固定为 user，即使请求里声明 admin 也无效） |
| `audit_log_file` | 未启用 MySQL 时的审计日志文件 |
| `mysql_*` | MySQL 连接与连接池配置 |

## 开发环境说明

Windows 端使用 Qt 6.11 + CMake/Ninja 构建桌面客户端（输出到项目内 `build-qt`，可用 `login.bat` 启动）；完整 Linux 服务端在 Linux/WSL 中安装 `protoc`、Protobuf 和 MySQL 开发库后构建，运行于 `0.0.0.0:9000`（MySQL 存储）。核心非网络逻辑和 Qt wire adapter 可以在 Windows 上验证，协议、线程池、用户服务测试源码位于 `tests/`。
