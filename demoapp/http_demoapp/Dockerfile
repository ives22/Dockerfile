FROM alpine

#CMD ["/bin/sh"]

RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories && apk update && apk --no-cache add python3 curl bind-tools iproute2 iptables ipvsadm py3-pip && pip3 install --no-cache-dir --upgrade pip -i https://mirrors.aliyun.com/pypi/simple/ && pip3 install --no-cache-dir -q Flask==2.0.2 requests==2.26.0 -i https://mirrors.aliyun.com/pypi/simple/

COPY demo.py /usr/local/bin/demo.py

ENV DEPLOYENV="Production" RELEASE="Stable" PS1="[\u@\h \w]\$ "

ENTRYPOINT ["/bin/sh", "-c", "python3 /usr/local/bin/demo.py"]
