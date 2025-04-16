FROM  golang:1.23.2 AS builder

WORKDIR /build

COPY . .

#RUN go mod download \
#  && go mod tidy \
#  && GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -o demoapp main.go

RUN go mod download \
  && go mod tidy \
  && CGO_ENABLED=0 go build -o demoapp server/main.go

FROM alpine

WORKDIR /opt

COPY --from=builder /build/demoapp /opt/demoapp

CMD ["/bin/sh", "-c", "/opt/demoapp"]