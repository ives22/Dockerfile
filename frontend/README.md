# Frontend 服务

这是一个前端代理服务，用于与后端 demoapp 服务进行交互。它提供了几个 API 端点，并将其他请求代理到后端服务。

## 功能特性

- 服务信息端点（`/services`）：连接后端的 `/api/get_service` 端点，获取后端服务信息
- 前端服务信息端点（`/api/get_service`）：返回前端服务的名称信息
- 代理功能：将除上述端点外的所有请求代理到后端服务，并根据需要传递特定请求头
- 响应增强：在所有代理响应中添加前端标识和请求耗时信息

## 环境变量

服务支持以下环境变量配置：

- `BACKEND_URL`：要连接访问的后端服务的 URL 端点，默认为 `http://localhost:80`
- `LISTEN`：监听的 IP 地址，默认为 `0.0.0.0`
- `PORT`：监听的端口，默认为 `5901`
- `PROXY_VERSION`：代理服务的版本标识，默认为 `v1.0`，会显示在所有代理响应中

## 构建和运行

### 本地运行

```bash
# 获取依赖
go mod tidy

# 构建应用
go build -o frontend .

# 运行应用
./frontend
```

### 使用 Docker

```bash
# 构建 Docker 镜像
docker build -t frontend .

# 运行容器（连接到默认后端）
docker run -p 5901:5901 frontend

# 运行容器（指定后端 URL）
docker run -p 5901:5901 -e BACKEND_URL=http://demoapp:80 frontend
```

## API 端点

### 服务信息

访问 `/services` 端点将连接到后端的 `/api/get_service` 端点，获取后端服务信息，并添加前端标识和耗时信息：

```bash
curl http://localhost:5901/services
```

响应示例：
```json
{
  "frontend_proxy": "v1.0",
  "elapsed_ms": 15,
  "backend_data": {
    "service_name": "demoapp",
    "instance_name": "demoapp-1234",
    "app_version": "v2.0"
  }
}
```

### 前端服务信息

访问 `/api/get_service` 端点将返回前端服务的名称信息：

```bash
curl http://localhost:5901/api/get_service
```

### 代理功能

所有其他请求都会被代理到后端服务，并在响应中添加前端标识和耗时信息。对于根路径（`/`）的请求，如果包含 `x-canary` 或 `x-user` 请求头，这些头将被传递给后端：

```bash
# 不传递特殊请求头
curl http://localhost:5901/
```

响应示例（HTML）：
```html
<!-- Frontend Proxy v1.0, Request Time: 12ms -->
<html>
<head><title>Welcome</title></head>
<body>
    <p>Demoapp by vvoo! App Version: v2.0, Client IP: 127.0.0.1, Server Name: demoapp-1234, Server IP: 10.0.0.1 ~</p>
</body>
</html>
```

响应示例（JSON）：
```json
{
  "frontend_proxy": "v1.0",
  "elapsed_ms": 8,
  "backend_data": {
    "original": "backend response"
  }
}
```

响应示例（纯文本）：
```
Frontend Proxy v1.0 | Request Time: 5ms
Original backend text response
```

```bash
# 传递 x-canary 请求头
curl -H "x-canary: true" http://localhost:5901/

# 传递 x-user 请求头
curl -H "x-user: testuser" http://localhost:5901/
```

## 在 Kubernetes 中部署

以下是在 Kubernetes 中部署前端服务的示例配置：

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: frontend
spec:
  replicas: 2
  selector:
    matchLabels:
      app: frontend
  template:
    metadata:
      labels:
        app: frontend
    spec:
      containers:
      - name: frontend
        image: your-registry/frontend:latest
        ports:
        - containerPort: 5901
        env:
        - name: BACKEND_URL
          value: "http://demoapp.default.svc.cluster.local"
        - name: PORT
          value: "5901"
        livenessProbe:
          httpGet:
            path: /api/get_service
            port: 5901
          initialDelaySeconds: 3
          periodSeconds: 3
---
apiVersion: v1
kind: Service
metadata:
  name: frontend
spec:
  selector:
    app: frontend
  ports:
  - port: 80
    targetPort: 5901
  type: ClusterIP
``` 