#!/bin/bash

# 创建构建器
# docker buildx create --name mybuilder --bootstrap --use

# 构建
docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -t vvoo/demoapp-grpc:v1.0 --push .

# 构建客户端
docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f Dockerfile-client -t vvoo/demoapp-grpc:client --push .

protoc --go_out=. --go_opt=paths=source_relative --go-grpc_out=. --go-grpc_opt=paths=source_relative data/demo.proto
