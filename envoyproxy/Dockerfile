FROM envoyproxy/envoy:v1.32.5

USER root

# 安装时区工具并设置默认时区（上海）
RUN apt-get update && \
    apt-get install -y tzdata && \
    ln -fs /usr/share/zoneinfo/UTC /etc/localtime && \
    echo "UTC" > /etc/timezone && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --chown=0:0 --chmod=755 docker-entrypoint.sh /

# 设置环境变量（可在运行时覆盖）
ENV TZ=UTC

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["envoy", "-c", "/etc/envoy/envoy.yaml"]
