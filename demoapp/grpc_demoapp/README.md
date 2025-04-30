# gRPC 示例应用

这是一个使用gRPC构建的示例应用，包含了多种gRPC通信模式的演示。

## 服务说明

### HelloWorld服务

HelloWorld服务演示了gRPC的四种通信模式：
1. 一元RPC (Unary RPC)：SayHello
2. 服务端流式RPC (Server Streaming)：Connect
3. 客户端流式RPC (Client Streaming)：UploadData
4. 双向流式RPC (Bidirectional Streaming)：Chat

### User服务

User服务提供了用户管理相关的功能：
1. 用户注册 (V1/V2)
2. 用户登录 (V1/V2)
3. 用户状态更新流 (V1/V2)

## 接口说明

### HelloWorld服务接口

```bash
# 查看服务列表
grpcurl -plaintext localhost:50051 list demo.HelloWorld
demo.HelloWorld.Chat
demo.HelloWorld.Connect
demo.HelloWorld.GetServerStats
demo.HelloWorld.SayHello
demo.HelloWorld.UploadData

# 查看User服务接口
grpcurl -plaintext localhost:50051 list demo.User
demo.User.GetUserUpdatesV1
demo.User.GetUserUpdatesV2
demo.User.LoginV1
demo.User.LoginV2
demo.User.RegisterV1
demo.User.RegisterV2
```

## 测试命令

### 测试HelloWorld服务

1. SayHello接口
```bash
grpcurl -plaintext -d '{"name": "test"}' localhost:50051 demo.HelloWorld/SayHello
# 或
docker run --name client -it --rm --network host vvoo/demoapp-grpc:client ./demoapp_client hello
```

2. Connect接口（服务器流）
```bash
grpcurl -plaintext -d '{"client_id": "test-client"}' localhost:50051 demo.HelloWorld/Connect
# 或
docker run --name client -it --rm --network host vvoo/demoapp-grpc:client ./demoapp_client connect
```

3. UploadData接口（客户端流）
```bash
# 使用客户端测试程序测试
# 使用客户端程序测试，因为grpcurl不支持流式上传
docker run --name client -it --rm --network host vvoo/demoapp-grpc:client ./demoapp_client upload
```

4. Chat接口（双向流）
```bash
# 使用客户端测试程序测试
docker run --name client -it --rm --network host vvoo/demoapp-grpc:client ./demoapp_client chat
```

5. GetServerStats接口
```bash
grpcurl -plaintext -d '{"client_id": "test-client"}' localhost:50051 demo.HelloWorld/GetServerStats
# 或
docker run --name client -it --rm --network host vvoo/demoapp-grpc:client ./demoapp_client stats
```

### 测试User服务

1. 注册接口V1
```bash
grpcurl -plaintext -d '{"username": "test", "password": "123456", "email": "test@example.com"}' localhost:50051 demo.User/RegisterV1
```

2. 注册接口V2
```bash
grpcurl -plaintext -d '{
  "username": "test",
  "password": "123456",
  "email": "test@example.com",
  "phone": "1234567890",
  "nickname": "测试用户",
  "metadata": {
    "avatar": "https://example.com/avatar.jpg",
    "description": "这是一个测试账号"
  }
}' localhost:50051 demo.User/RegisterV2
```

3. 登录接口V1
```bash
grpcurl -plaintext -d '{"username": "test", "password": "123456"}' localhost:50051 demo.User/LoginV1
```

4. 登录接口V2（用户名登录）
```bash
grpcurl -plaintext -d '{
  "identifier": "test",
  "credential": "123456",
  "login_type": "USERNAME"
}' localhost:50051 demo.User/LoginV2
```

5. 登录接口V2（邮箱登录）
```bash
grpcurl -plaintext -d '{
  "identifier": "test@example.com",
  "credential": "123456",
  "login_type": "EMAIL"
}' localhost:50051 demo.User/LoginV2
```

6. 登录接口V2（手机号登录）
```bash
grpcurl -plaintext -d '{
  "identifier": "1234567890",
  "credential": "123456",
  "login_type": "PHONE"
}' localhost:50051 demo.User/LoginV2
```

## 项目结构

```
.
├── README.md          # 项目说明文档
├── go.mod            # Go模块定义
├── go.sum            # Go依赖版本锁定
├── data/             # 协议定义和生成的代码
│   ├── demo.proto     # 服务定义
│   ├── demo.pb.go     # 生成的消息代码
│   └── demo_grpc.pb.go# 生成的服务代码
├── server/           # 服务器端代码
│   ├── main.go       # 服务器入口
│   └── services/     # 服务实现
│       └── user_service.go # User服务实现
├── client/           # 客户端代码
│   ├── main.go       # 客户端入口
│   └── services/     # 客户端服务
│       └── user_client.go  # User服务客户端
└── build.sh          # 构建脚本
```

## 构建和运行

### 生成gRPC代码
```bash
protoc --go_out=. --go_opt=paths=source_relative \
    --go-grpc_out=. --go-grpc_opt=paths=source_relative \
    data/demo.proto
```

### 构建和运行服务器
```bash
# 构建服务器镜像
docker build -t grpc-demo-server .

# 运行服务器
docker run -p 50051:50051 grpc-demo-server
```

### 构建和运行客户端
```bash
# 构建客户端镜像
docker build -f Dockerfile-client -t grpc-demo-client .

# 运行客户端
docker run --network host grpc-demo-client
```

## 版本说明

### 文件版本
- data/demo.proto: v1.0.0

### 服务版本
- HelloWorld服务：v1.0.0
- User服务：v1.0.0

## 依赖说明

- Go 1.20+
- gRPC
- Protocol Buffers v3
- JWT
- bcrypt 