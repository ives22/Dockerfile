# 第一阶段：构建阶段
FROM golang:1.24-alpine AS builder

# 设置工作目录
WORKDIR /app

# 设置国内代理
ENV GOPROXY=https://goproxy.cn,direct
ENV GO111MODULE=on

# 复制 go mod 文件
COPY go.mod go.sum* ./

# 下载依赖
RUN go mod download

# 复制源代码
COPY . .

# 构建应用
RUN CGO_ENABLED=0 GOOS=linux go build -o demoapp .

# 第二阶段：运行阶段
FROM alpine:latest

# 安装基本工具
RUN apk --no-cache add ca-certificates tzdata

# 设置时区为亚洲/上海
ENV TZ=Asia/Shanghai

# 创建非 root 用户
RUN adduser -D -g '' appuser

# 从构建阶段复制二进制文件
COPY --from=builder /app/demoapp /usr/local/bin/

# 使用非 root 用户运行
USER appuser

# 设置环境变量
ENV VERSION=v2.0
ENV APP_NAME=demoapp
ENV PORT=80

# 暴露端口
EXPOSE 80

# 运行应用
ENTRYPOINT ["/usr/local/bin/demoapp"] 