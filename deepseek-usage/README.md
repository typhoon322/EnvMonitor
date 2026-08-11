# DeepSeek 用量监控

独立 Web 服务：在 Web UI 中管理多个 DeepSeek API Key（名称 + Key），展示各 Key 的**余额与可用性**。

> DeepSeek 官方 Bearer API 目前提供 `GET /user/balance`（余额），不提供按 Token 的历史用量明细。本模块展示的是余额看板，便于多 Key 统一查看。

## 功能

- 多 Key 管理：添加 / 编辑 / 删除，自定义名称
- Key 本地加密存储（Fernet，`MASTER_KEY`）
- 手动刷新 + 定时自动刷新
- 单容器 Docker 部署，与 ESP32 固件解耦

## 快速开始

### 1. 生成 MASTER_KEY

```bash
python -c "from cryptography.fernet import Fernet; print(Fernet.generate_key().decode())"
```

### 2. 配置环境变量

```bash
cp .env.example .env
# 编辑 .env，填入 MASTER_KEY
```

### 3. Docker 部署（推荐）

```bash
docker compose up -d --build
```

浏览器打开：`http://localhost:8787`

### 4. 本地开发

```bash
cd deepseek-usage
cp .env.example .env
# 编辑 .env，填入 MASTER_KEY
./scripts/dev.sh
```

或手动：

```bash
cd backend
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
export MASTER_KEY='your-fernet-key'
python run.py
```

## API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/health` | 健康检查 |
| GET | `/api/keys` | Key 列表（不含明文） |
| POST | `/api/keys` | 添加 Key `{name, api_key}` |
| PUT | `/api/keys/{id}` | 更新名称或 Key |
| DELETE | `/api/keys/{id}` | 删除 Key |
| GET | `/api/balances` | 缓存的余额快照 |
| POST | `/api/balances/refresh` | 刷新全部 |
| POST | `/api/balances/{id}/refresh` | 刷新单个 |

## 环境变量

| 变量 | 必填 | 默认 | 说明 |
|------|------|------|------|
| `MASTER_KEY` | 是 | — | Fernet 密钥，用于加密存储 API Key |
| `HOST` | 否 | `0.0.0.0` | 监听地址 |
| `PORT` | 否 | `8787` | 监听端口 |
| `DATA_DIR` | 否 | `./data` | SQLite 数据目录 |
| `REFRESH_INTERVAL_SEC` | 否 | `300` | 后台自动刷新间隔（秒） |

## 目录结构

```
deepseek-usage/
├── backend/          # FastAPI 服务
├── frontend/         # 静态 Web UI
├── docker-compose.yml
├── Dockerfile
└── README.md
```

## 安全提示

- `MASTER_KEY` 丢失后无法解密已存 Key，请妥善备份
- 建议仅在内网或反向代理后访问，不要暴露到公网
- 本服务不会把 API Key 回传给前端，仅显示掩码（如 `sk-a...xyz`）
