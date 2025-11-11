# FROM ubuntu:24.04

# # 安装 locales 包
# RUN apt-get update && \
#     apt-get install -y --no-install-recommends locales tzdata && \
#     rm -rf /var/lib/apt/lists/*

# # 生成 en_US.UTF-8 本地化语言环境
# RUN locale-gen zh_CN.UTF-8

# # 设置语言环境和时区（UTC）
# ENV LANG=zh_CN.UTF-8 \
#     LANGUAGE=zh_CN:zh \
#     LC_ALL=zh_CN.UTF-8

# # 链接 UTC 时区到系统默认时区
# RUN ln -sf /usr/share/zoneinfo/Etc/UTC /etc/localtime && echo "UTC" > /etc/timezone

# CMD ["/bin/bash"]



FROM ubuntu:24.04

# 设置构建时的环境变量
ENV LANG=zh_CN.UTF-8 \
    LANGUAGE=zh_CN:zh \
    LC_ALL=zh_CN.UTF-8 \
    TZ=Etc/UTC

# 一次性执行所有 apt 操作
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        locales \
        tzdata \
        ca-certificates \
        && \
    # 生成所需的本地化文件
    locale-gen zh_CN.UTF-8 && \
    # 配置时区（通过环境变量 TZ 自动设置）
    ln -sf /usr/share/zoneinfo/Etc/UTC /etc/localtime && \
    echo "UTC" > /etc/timezone && \
    # 清理缓存以减小镜像大小
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# 创建非 root 用户（安全最佳实践）
RUN groupadd -r appuser && useradd -r -g appuser appuser

# 建议在运行应用时切换用户，而不是在基础镜像中
# USER appuser

CMD ["/bin/bash"]



# docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f Dockerfile -t registry-intl.ap-southeast-1.aliyuncs.com/relax_many_test/ubuntu:24.04 --push .
# docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f Dockerfile -t registry-intl.ap-southeast-1.aliyuncs.com/relax_many/ubuntu-base:v1.0.0 --push .
