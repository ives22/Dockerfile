# 基础镜像
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

# 入口命令
CMD [ "sh", "-c", "ln -sf /usr/share/zoneinfo/$TZ /etc/localtime && exec sh" ]

