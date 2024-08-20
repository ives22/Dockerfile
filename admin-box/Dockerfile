FROM alpine

CMD ["/bin/sh"]

LABEL maintainer=vvoo <vvoo@ives.com> VERSION=v1.0

RUN /bin/sh -c sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories && apk update && apk --no-cache add bash curl bind-tools iproute2 iptables ipvsadm nmap tcpdump netcat-openbsd openssl shadow 

ENV DEPLOYENV="Production" RELEASE="Stable" PS1="[\u@\[\e[0;31m\]\h\[\e[0m\] \[\e[0;36m\]\w\[\e[0m\]]\\$ "

#ENV DEPLOYENV="Production" RELEASE="Stable" PS1="[\u@\h \w]\$ "

ENTRYPOINT ["/bin/sh", "-c", "sleep 999999"]
