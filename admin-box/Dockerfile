FROM alpine:latest

CMD ["/bin/sh"]

LABEL maintainer="vvoo <vvoo@ives.com>" \
      version="v1.0" \
      description="Network troubleshooting container"

ENV DEPLOYENV="Production" \
    RELEASE="Stable" \
    PS1="[\u@\[\e[0;31m\]\h\[\e[0m\] \[\e[0;36m\]\w\[\e[0m\]]\\$ "

RUN set -eux && \
    sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories && \
    apk update && \
    apk add --no-cache \
        bash \
        curl \
        bind-tools \
        iproute2 \
        iptables \
        ipvsadm \
        nmap \
        tcpdump \
        netcat-openbsd \
        openssl \
        shadow && \
    rm -rf /var/cache/apk/*

ENTRYPOINT ["sleep", "infinity"]
