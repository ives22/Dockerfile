# 基础镜像 用于go服务运行 基础环境镜像
FROM alpine:latest

# 安装所需工具和时区数据
RUN apk add --no-cache tzdata font-noto-cjk bash vim curl busybox-extras musl-locales musl-locales-lang \
    && update-ca-certificates \
    && ln -sf /usr/share/zoneinfo/UTC /etc/localtime \
    && echo "UTC" > /etc/timezone \
    && echo "LANG=zh_CN.UTF-8" >> /etc/profile.d/locale.sh \
    && echo "export LANG" >> /etc/profile.d/locale.sh

# 允许运行时指定 TZ 时区
ENV TZ=UTC

# 默认环境变量  --- api 项目
ENV API_ENV="dev"
ENV API_NACOS="nacos-svc-clusterip.dev.svc.cluster.local:8848"
ENV API_ID="0"

# 入口命令
CMD [ "sh", "-c", "ln -sf /usr/share/zoneinfo/$TZ /etc/localtime && exec sh" ]



# docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f Dockerfile -t registry-intl.ap-southeast-1.aliyuncs.com/relax_many/base-env:v1.0.3 --push .
