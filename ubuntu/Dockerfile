FROM ubuntu:24.04

# 安装 locales 包
RUN apt-get update && \
    apt-get install -y --no-install-recommends locales && \
    rm -rf /var/lib/apt/lists/*

# 生成 en_US.UTF-8 本地化语言环境
RUN locale-gen zh_CN.UTF-8

# 设置环境变量
ENV LANG=zh_CN.UTF-8 \
    LANGUAGE=zh_CN:zh \
    LC_ALL=zh_CN.UTF-8

CMD ["/bin/bash"]


# docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f Dockerfile -t registry-intl.ap-southeast-1.aliyuncs.com/relax_many_test/ubuntu:24.04 --push .

