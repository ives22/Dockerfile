FROM alpine


# ENV PACKAGES='make gcc musl-dev linux-headers'

RUN   apk add --update wget make gcc musl-dev linux-headers &&   cd /root &&  wget https://github.com/ColinIanKing/stress-ng/archive/refs/tags/V0.14.00.tar.gz &&   tar -xzf  V0.14.00.tar.gz &&   cd stress-ng-0.14.00/ &&   make install &&   apk del make gcc musl-dev linux-headers


#ENTRYPOINT ["/usr/bin/stress-ng"]
CMD ["/usr/bin/stress-ng", "--help"]

