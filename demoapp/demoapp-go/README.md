# Demoapp Go

用于 Kubernetes、Service、Ingress、Gateway API 和服务网格验证的多协议测试后端。默认提供 HTTP 服务；HTTPS/mTLS、gRPC、TCP 和 UDP 通过环境变量按需启用。应用不访问 Kubernetes API，Pod、Service 和入口信息均由环境变量注入。

## HTTP 接口

| 路径 | 方法 | 用途 |
| --- | --- | --- |
| `/` | GET | 欢迎页、客户端 IP 和实例主机名 |
| `/livez`、`/readyz` | GET/POST/HEAD | 存活、就绪探针以及 Endpoint 摘除测试 |
| `/request`、`/request/*path` | GET/POST/PUT/PATCH/DELETE/OPTIONS/HEAD | 请求行、Header、Query、代理链路和路径改写验证 |
| `/ip` | GET | 客户端、TCP 对端、Pod 和 Service IP |
| `/delay?seconds=5` | GET | 随机或固定延迟，验证超时 |
| `/status/:code` | GET | 确定性返回 `200-599` 状态码 |
| `/flaky` | GET | 前 N 次失败后成功，验证重试和熔断 |
| `/stats` | GET | 当前 Pod 请求数、状态码和并发快照 |
| `/metrics` | GET | Prometheus Go 与 Demoapp 指标 |
| `/echo` | POST | 回显最多 1 MiB 的文本或 base64 二进制请求体 |
| `/stream` | GET | NDJSON 分块流式响应 |
| `/bytes` | GET | 返回指定大小的二进制响应，最大 10 MiB |
| `/cookie` | GET | 设置、读取或删除 Cookie，验证会话保持 |
| `/ws` | GET Upgrade | WebSocket Echo |
| `/stress/cpu`、`/stress/memory` | GET | Token 保护的 CPU/内存压力测试 |
| `/api/get_service`、`/hostname`、`/configs`、`/user-agent` | GET | 兼容已有接口 |

所有 HTTP 响应都会携带以下诊断头：

- `X-Demo-Pod`
- `X-Demo-Pod-Ip`
- `X-Demo-Version`
- `X-Demo-Request-Id`

请求已有 `X-Request-Id` 时会透传，否则应用生成请求 ID。任何未注册的路径或方法仍按现有约定返回 `/` 的内容和 `200`。

### 请求和路径诊断

```bash
curl -H 'X-Canary: true' 'http://localhost:8080/request/origin/fullpath?a=1&a=2'
```

`/request` 和 `/ip` 默认返回缩进 JSON。`clientIP` 是 Gin 按可信代理配置解析的客户端地址，`remoteIP` 是 TCP 直连对端地址。未配置 `TRUSTED_PROXIES` 时不会信任客户端提供的转发头。

### 状态码、重试和熔断

```bash
# 确定性返回 503
curl -i http://localhost:8080/status/503

# 同一 Pod 内，key=retry-demo 的前两次请求返回 503，第三次起返回 200
curl 'http://localhost:8080/flaky?key=retry-demo&failures=2&status=503'
```

`/flaky` 的计数是单 Pod 内存状态，最多保留 10,000 个不同 key；达到上限后新 key 返回 `429`，已有 key 不受影响。测试 Gateway 重试时应结合 `X-Demo-Pod`、`X-Demo-Request-Id`、应用日志和 `/stats` 判断是否发生了重试及是否换了后端。

### 延迟和超时

```bash
# 不传 seconds 时随机延迟 0-3000 毫秒
curl http://localhost:8080/delay

# 固定延迟 5 秒，允许 0-300 的整数
curl 'http://localhost:8080/delay?seconds=5'
```

响应保留兼容字段 `requested_delay_ms`、`actual_delay_ms` 和 `timestamp`。非法参数返回 `400 INVALID_DELAY_SECONDS`。

### Body、流式响应和大响应

```bash
curl -X POST -H 'Content-Type: text/plain' --data 'hello' http://localhost:8080/echo
curl -N 'http://localhost:8080/stream?chunks=5&intervalMs=1000'
curl -o /dev/null 'http://localhost:8080/bytes?size=1048576'
```

- `/echo` 最大读取 1 MiB；不安全的二进制内容以 base64 返回。
- `/stream` 支持 `chunks=1-100`、`intervalMs=0-10000`，并禁用常见代理缓冲头。
- `/bytes` 支持 `size=0-10485760`。

### Cookie 与 WebSocket

```bash
curl -c cookies.txt 'http://localhost:8080/cookie?action=set&name=route&value=pod-a'
curl -b cookies.txt 'http://localhost:8080/cookie?action=read&name=route'

# 使用 websocat 验证 WebSocket
websocat ws://localhost:8080/ws
```

WebSocket 保留 Gorilla 的默认同源 Origin 校验，并在进程优雅关闭时发送 Going Away 关闭帧。

### 指标与统计

```bash
curl http://localhost:8080/stats
curl http://localhost:8080/metrics
```

自定义 Prometheus 指标：

- `demoapp_http_requests_total{pod,status}`
- `demoapp_http_current_requests{pod}`
- `demoapp_http_max_concurrency{pod}`

`/stats` 和 `/metrics` 都是单 Pod 视角；经负载均衡访问时应从每个 Pod 抓取或由 Prometheus 聚合。

## 多协议服务

多协议端口默认不启动。设置对应端口环境变量后启用：

| 环境变量 | 示例 | 功能 |
| --- | --- | --- |
| `HTTPS_PORT` | `8443` | HTTPS 后端，支持 HTTP/1.1 和 HTTP/2 ALPN |
| `TLS_CERT_FILE`、`TLS_KEY_FILE` | `/tls/tls.crt`、`/tls/tls.key` | HTTPS 服务端证书，启用 HTTPS 时必填 |
| `TLS_CLIENT_CA_FILE` | `/tls/ca.crt` | 配置后要求并校验客户端证书，即 mTLS |
| `GRPC_PORT` | `9090` | gRPC Echo、标准 Health 和 Reflection |
| `TCP_PORT` | `9000` | TCP Echo |
| `UDP_PORT` | `9000` | UDP Echo |

### gRPC

服务定义位于 `proto/demo/v1/demo.proto`，服务名为 `demo.v1.DemoService`。

```bash
grpcurl -plaintext localhost:9090 list
grpcurl -plaintext -d '{"message":"hello"}' localhost:9090 demo.v1.DemoService/Echo
grpcurl -plaintext -d '{}' localhost:9090 grpc.health.v1.Health/Check
```

### TCP 和 UDP

```bash
printf 'hello tcp' | nc localhost 9000
printf 'hello udp' | nc -u -w1 localhost 9000
```

TCP 单连接最多回显 10 MiB，UDP 单报文受标准 UDP 最大报文限制。

## 压力测试

压力接口默认禁用，只有设置 `STRESS_TOKEN` 后才启用。请求必须携带 `X-Stress-Token`。

```bash
curl -H 'X-Stress-Token: change-me' 'http://localhost:8080/stress/cpu?milliseconds=1000'
curl -H 'X-Stress-Token: change-me' 'http://localhost:8080/stress/memory?megabytes=64&holdSeconds=10'
```

- CPU：`milliseconds=1-10000`
- 内存：`megabytes=1-256`，`holdSeconds=0-60`

不要将 Token 暴露给不可信客户端。建议仅在隔离测试环境启用，并同时设置容器 requests/limits。

## 环境变量

| 变量 | 默认值或来源 |
| --- | --- |
| `VERSION` | 镜像默认 `v2.1`；可在部署时覆盖 |
| `APP_NAME` | `demoapp` |
| `HOST` | `0.0.0.0` |
| `PORT` | `80` |
| `POD_NAME`、`POD_IP`、`NAMESPACE` | 建议使用 Downward API |
| `SERVICE_NAME`、`SERVICE_IP`、`INGRESS_NAME` | 部署清单、ConfigMap 或发布系统注入 |
| `TRUSTED_PROXIES` | 逗号分隔的可信代理 IP/CIDR；未设置或配置无效时不信任任何转发头 |
| `STRESS_TOKEN` | 未设置时压力接口返回 404 |

Service ClusterIP 不能通过 Pod Downward API 获取，必须显式注入。未设置的 Kubernetes 元数据返回空字符串；应用不会查询 Kubernetes API、解析 DNS 或扫描网卡。

## Kubernetes 示例

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
      terminationGracePeriodSeconds: 35
      containers:
      - name: demoapp-go
        image: your-registry/demoapp-go:latest
        ports:
        - {name: http, containerPort: 80}
        - {name: grpc, containerPort: 9090}
        - {name: tcp-echo, containerPort: 9000, protocol: TCP}
        - {name: udp-echo, containerPort: 9000, protocol: UDP}
        env:
        - {name: VERSION, value: "v3.0"}
        - {name: APP_NAME, value: demoapp-go}
        - {name: SERVICE_NAME, value: demoapp-go}
        - {name: SERVICE_IP, value: "10.96.10.20"}
        - {name: INGRESS_NAME, value: demoapp-route}
        - {name: TRUSTED_PROXIES, value: "10.0.0.0/8"}
        - {name: GRPC_PORT, value: "9090"}
        - {name: TCP_PORT, value: "9000"}
        - {name: UDP_PORT, value: "9000"}
        - name: POD_NAME
          valueFrom: {fieldRef: {fieldPath: metadata.name}}
        - name: POD_IP
          valueFrom: {fieldRef: {fieldPath: status.podIP}}
        - name: NAMESPACE
          valueFrom: {fieldRef: {fieldPath: metadata.namespace}}
        readinessProbe:
          httpGet: {path: /readyz, port: http}
          periodSeconds: 3
        livenessProbe:
          httpGet: {path: /livez, port: http}
          periodSeconds: 3
---
apiVersion: v1
kind: Service
metadata:
  name: demoapp-go
spec:
  selector:
    app: demoapp-go
  ports:
  - {name: http, port: 80, targetPort: http}
  - {name: grpc, port: 9090, targetPort: grpc}
  - {name: tcp-echo, port: 9000, targetPort: tcp-echo, protocol: TCP}
  - {name: udp-echo, port: 9000, targetPort: udp-echo, protocol: UDP}
```

进程收到 `SIGTERM`/`SIGINT` 后会先把 Readiness 标为失败，再并发关闭 HTTP/HTTPS、WebSocket、gRPC、TCP 和 UDP 服务，整个流程共享最长 30 秒的关闭窗口；活动 TCP/WebSocket 连接会被关闭。

## 可验证场景

- Service：多副本负载均衡、ClientIP 会话亲和、内部/外部流量策略、拓扑偏好。
- Ingress/HTTPRoute：Host、Path、Method、Header、Query 匹配，路径改写、Header 修改、重定向和权重路由。
- Gateway/Service Mesh：请求/后端超时、重试、熔断、限流、请求镜像和并发限制。
- 发布与弹性：Readiness Endpoint 摘除、滚动发布、优雅连接排空、HPA、资源限制和 OOM。
- 协议：HTTPS/mTLS、GRPCRoute、WebSocket、TCPRoute 和 UDPRoute。

限流、重试和熔断由具体 Gateway/Ingress 实现配置；Demoapp 提供确定性失败、状态码、实例身份、请求 ID、统计和指标作为验证证据，不在应用内替代数据平面策略。

## 构建运行

```bash
go test ./...
go build -o demoapp .
./demoapp --port 8080

docker build -t demoapp-go .
docker run --rm -p 8080:80 demoapp-go
```
