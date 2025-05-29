# Demoapp Go 版本

这是一个用 Go 语言实现的演示应用程序，基于原始的 Python 版本 `demo.py` 进行移植。该应用提供了多个 HTTP 端点，用于演示和测试 Kubernetes 环境中的各种功能。

## 功能特性

- 欢迎页面（根路径 `/`）：显示服务器信息和客户端信息
- 健康检查端点（`/livez` 和 `/readyz`）：用于 Kubernetes 的存活和就绪探针
- 服务信息 API（`/api/get_service`）：返回服务名称、实例名称和版本信息
- 主机名端点（`/hostname`）：显示服务器主机名
- 配置信息端点（`/configs`）：显示环境变量中的部署环境和发布信息
- 用户代理端点（`/user-agent`）：显示客户端的 User-Agent 信息
- 延迟端点（`/delay`）：模拟随机延迟（0-3000毫秒），用于测试超时场景

## 环境变量

应用程序支持以下环境变量：

- `VERSION`：应用版本，默认为 `v2.0`
- `APP_NAME`：应用名称，默认为 `demoapp`
- `HOST`：监听主机，默认为 `0.0.0.0`
- `PORT`：监听端口，默认为 `80`
- `DEPLOYENV`：部署环境信息
- `RELEASE`：发布版本信息

## 构建和运行

### 本地运行

```bash
# 获取依赖
go mod tidy

# 构建应用
go build -o demoapp .

# 运行应用
./demoapp
```

### 命令行参数

应用支持以下命令行参数：

- `-p, --port <端口号>`：指定监听端口
- `-l, --host <主机地址>`：指定监听地址
- `-v, --verbose`：启用调试模式

### 使用 Docker

```bash
# 构建 Docker 镜像
docker build -t demoapp-go .

# 运行容器
docker run -p 8080:80 demoapp-go

# 使用环境变量
docker run -p 8080:80 -e VERSION=v3.0 -e APP_NAME=my-app demoapp-go
```

## API 端点

### 健康检查 API

#### 存活探针

```bash
# 获取存活状态
curl http://localhost:8080/livez

# 设置存活状态
curl -X POST -d "livez=OK" http://localhost:8080/livez
curl -X POST -d "livez=ERROR" http://localhost:8080/livez
```

#### 就绪探针

```bash
# 获取就绪状态
curl http://localhost:8080/readyz

# 设置就绪状态
curl -X POST -d "readyz=OK" http://localhost:8080/readyz
curl -X POST -d "readyz=ERROR" http://localhost:8080/readyz
```

### 延迟测试 API

```bash
# 测试随机延迟
curl http://localhost:8080/delay
```

响应示例：
```json
{
  "requested_delay_ms": 1234,
  "actual_delay_ms": 1235,
  "timestamp": "2023-05-29T12:34:56+08:00"
}
```

## 在 Kubernetes 中部署

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: demoapp-go
spec:
  replicas: 3
  selector:
    matchLabels:
      app: demoapp-go
  template:
    metadata:
      labels:
        app: demoapp-go
    spec:
      containers:
      - name: demoapp-go
        image: your-registry/demoapp-go:latest
        ports:
        - containerPort: 80
        env:
        - name: VERSION
          value: "v2.0"
        - name: APP_NAME
          value: "demoapp-go"
        livenessProbe:
          httpGet:
            path: /livez
            port: 80
          initialDelaySeconds: 3
          periodSeconds: 3
        readinessProbe:
          httpGet:
            path: /readyz
            port: 80
          initialDelaySeconds: 3
          periodSeconds: 3
``` 