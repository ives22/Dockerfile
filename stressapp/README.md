Kubernetes 资源模拟器 API 文档
# 概述
一个用于模拟 CPU/Memory/Disk 负载的 Go 程序，提供 Prometheus 指标暴露和动态探针控制，专为 Kubernetes 测试场景设计。

# 快速开始
```bash
# 运行程序
go run main.go

# 构建 Docker 镜像
docker build -t stressapp .
```

# 接口列表
## 基础健康检查

| 端点       | 方法 | 描述                 | 参数 | 示例响应                        |
| :--------- | :--- | :------------------- | :--- | :------------------------------ |
| `/healthz` | GET  | 存活探针（动态可调） | 无   | `OK` (200) 或 `Unhealthy` (503) |
| `/readyz`  | GET  | 就绪探针（动态可调） | 无   | `OK` (200) 或 `Not Ready` (503) |

## 资源模拟接口

### CPU 密集型

```
GET /cpu
```

参数:

- seconds : 持续时间（秒，默认10）
- intensity : CPU 负载百分比（1-100，默认100）

示例:

```bash
curl "http://localhost:8080/cpu?seconds=30&intensity=80"
```

响应:

```bash
{
  "status": "ok",
  "message": "CPU intensive task started for 30 seconds with intensity 80%",
  "duration": "30s"
}
```

### 内存密集型

```bash
GET /memory
```

参数:

- `mb` : 分配内存大小（MB，默认100）
- `seconds` : 保持时间（秒，默认30）

示例:

```bash
curl "http://localhost:8080/memory?mb=200&seconds=60"
```

响应:

```bash
{
  "status": "ok",
  "message": "Allocated 200 MB of memory for 60 seconds",
  "duration": "60s"
}
```

### 组合负载

```bash
GET /combined
```

参数:

- `cpu_seconds` + `cpu_intensity` : CPU 负载参数
- `memory_mb` + `memory_seconds` : 内存负载参数

示例:

```bash
curl "http://localhost:8080/combined?cpu_seconds=20&cpu_intensity=70&memory_mb=150&memory_seconds=40"
```

### 磁盘 I/O

```bash
GET /diskio
```

参数:

- `mb` : 单次读写量（MB，默认100）
- `iterations` : 循环次数（默认10）

示例:

```bash
curl "http://localhost:8080/diskio?mb=500&iterations=5"
```

## 探针控制接口

| 端点                 | 方法 | 功能             | 参数             | 示例命令                                                     |
| :------------------- | :--- | :--------------- | :--------------- | :----------------------------------------------------------- |
| `/probe/healthz/set` | GET  | 设置存活探针状态 | `status=ok/fail` | `curl "http://localhost:8080/probe/healthz/set?status=fail"` |
| `/probe/readyz/set`  | GET  | 设置就绪探针状态 | `status=ok/fail` | `curl "http://localhost:8080/probe/readyz/set?status=ok"`    |
| `/probe/status`      | GET  | 查看当前探针状态 | 无               | `curl http://localhost:8080/probe/status`                    |

## 监控指标

```bash
GET /metrics
```

**暴露的 Prometheus 指标**:

- `app_cpu_load_percent` : 当前 CPU 负载百分比
- `app_memory_usage_bytes` : 内存使用量（字节）
- `app_requests_total` : 总请求计数器



## 测试场景示例

1. **触发 HPA 扩容**:

```bash
# 持续制造 CPU 负载
while true; do curl "http://<service>/cpu?intensity=90&seconds=300"; done
```

2. **模拟 Pod 不健康**:

```bash
# 1. 设置探针为失败状态
curl "http://<service>/probe/healthz/set?status=fail"

# 2. 观察 Kubernetes 事件（约 15-30 秒后会重启 Pod）
kubectl get events -w
```

