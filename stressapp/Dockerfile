FROM golang:alpine AS builder
RUN apk add --no-cache ca-certificates \
    make \
    git
COPY . /go-build
RUN cd /go-build && \
    export GO111MODULE=on && \
    export GOPROXY=https://goproxy.io && \
    go build -o 'stressapp'

FROM alpine
RUN apk add --no-cache tzdata

COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs
COPY --from=builder /go-build/stressapp /usr/bin/stressapp

ENV TZ=Asia/Shanghai

CMD ["stressapp"]
