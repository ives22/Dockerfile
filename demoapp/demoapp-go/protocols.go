package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"errors"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strconv"
	"sync"
	"time"

	demov1 "demoapp/proto/demo/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/health"
	"google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/reflection"
)

type demoGRPCServer struct {
	demov1.UnimplementedDemoServiceServer
	config diagnosticConfig
}

func (server *demoGRPCServer) Echo(_ context.Context, request *demov1.EchoRequest) (*demov1.EchoResponse, error) {
	return &demov1.EchoResponse{
		Message: request.GetMessage(),
		Pod:     server.config.Pod,
		PodIp:   server.config.PodIP,
		Version: server.config.Version,
	}, nil
}

type grpcServerRuntime struct {
	server *grpc.Server
	health *health.Server
}

func newGRPCServer(config diagnosticConfig) *grpcServerRuntime {
	server := grpc.NewServer()
	demov1.RegisterDemoServiceServer(server, &demoGRPCServer{config: config})
	healthServer := health.NewServer()
	healthServer.SetServingStatus("", grpc_health_v1.HealthCheckResponse_SERVING)
	healthServer.SetServingStatus(demov1.DemoService_ServiceDesc.ServiceName, grpc_health_v1.HealthCheckResponse_SERVING)
	grpc_health_v1.RegisterHealthServer(server, healthServer)
	reflection.Register(server)
	return &grpcServerRuntime{server: server, health: healthServer}
}

func (runtime *grpcServerRuntime) Serve(listener net.Listener) error {
	return runtime.server.Serve(listener)
}

func (runtime *grpcServerRuntime) GracefulStop() {
	runtime.health.Shutdown()
	runtime.server.GracefulStop()
}

func (runtime *grpcServerRuntime) Stop() {
	runtime.health.Shutdown()
	runtime.server.Stop()
}

type tcpEchoServer struct {
	listener    net.Listener
	closed      chan struct{}
	once        sync.Once
	mu          sync.Mutex
	connections map[net.Conn]struct{}
}

func newTCPEchoServer(listener net.Listener) *tcpEchoServer {
	return &tcpEchoServer{
		listener:    listener,
		closed:      make(chan struct{}),
		connections: make(map[net.Conn]struct{}),
	}
}

func (server *tcpEchoServer) Serve() {
	for {
		connection, err := server.listener.Accept()
		if err != nil {
			select {
			case <-server.closed:
				return
			default:
				log.Printf("TCP Echo 接收连接失败: %v", err)
				continue
			}
		}
		if !server.registerConnection(connection) {
			_ = connection.Close()
			return
		}
		go func() {
			defer func() {
				server.unregisterConnection(connection)
				_ = connection.Close()
			}()
			_, _ = io.Copy(connection, io.LimitReader(connection, 10<<20))
		}()
	}
}

func (server *tcpEchoServer) Close() error {
	var closeError error
	server.once.Do(func() {
		close(server.closed)
		closeError = server.listener.Close()

		server.mu.Lock()
		connections := make([]net.Conn, 0, len(server.connections))
		for connection := range server.connections {
			connections = append(connections, connection)
		}
		server.mu.Unlock()
		for _, connection := range connections {
			_ = connection.Close()
		}
	})
	return closeError
}

func (server *tcpEchoServer) registerConnection(connection net.Conn) bool {
	server.mu.Lock()
	defer server.mu.Unlock()
	select {
	case <-server.closed:
		return false
	default:
		server.connections[connection] = struct{}{}
		return true
	}
}

func (server *tcpEchoServer) unregisterConnection(connection net.Conn) {
	server.mu.Lock()
	defer server.mu.Unlock()
	delete(server.connections, connection)
}

type udpEchoServer struct {
	connection *net.UDPConn
	closed     chan struct{}
	once       sync.Once
}

func newUDPEchoServer(connection *net.UDPConn) *udpEchoServer {
	return &udpEchoServer{connection: connection, closed: make(chan struct{})}
}

func (server *udpEchoServer) Serve() {
	buffer := make([]byte, 64<<10)
	for {
		size, address, err := server.connection.ReadFromUDP(buffer)
		if err != nil {
			select {
			case <-server.closed:
				return
			default:
				log.Printf("UDP Echo 读取失败: %v", err)
				continue
			}
		}
		if _, err := server.connection.WriteToUDP(buffer[:size], address); err != nil {
			log.Printf("UDP Echo 写入失败: %v", err)
		}
	}
}

func (server *udpEchoServer) Close() error {
	server.once.Do(func() { close(server.closed) })
	return server.connection.Close()
}

func loadTLSConfig(certFile, keyFile, clientCAFile string) (*tls.Config, error) {
	certificate, err := tls.LoadX509KeyPair(certFile, keyFile)
	if err != nil {
		return nil, err
	}
	config := &tls.Config{
		MinVersion:   tls.VersionTLS12,
		Certificates: []tls.Certificate{certificate},
	}
	if clientCAFile == "" {
		return config, nil
	}
	caPEM, err := os.ReadFile(clientCAFile)
	if err != nil {
		return nil, err
	}
	clientCAs := x509.NewCertPool()
	if !clientCAs.AppendCertsFromPEM(caPEM) {
		return nil, errors.New("TLS_CLIENT_CA_FILE 不包含有效证书")
	}
	config.ClientCAs = clientCAs
	config.ClientAuth = tls.RequireAndVerifyClientCert
	return config, nil
}

type optionalProtocolServers struct {
	httpsServer *http.Server
	grpcServer  *grpcServerRuntime
	tcpServer   *tcpEchoServer
	udpServer   *udpEchoServer
}

func startOptionalProtocolServers(host string, handler http.Handler, config diagnosticConfig) (*optionalProtocolServers, error) {
	servers := &optionalProtocolServers{}
	cleanup := func() { servers.Close() }

	if port, enabled, err := optionalPort("HTTPS_PORT"); err != nil {
		return nil, err
	} else if enabled {
		certFile := os.Getenv("TLS_CERT_FILE")
		keyFile := os.Getenv("TLS_KEY_FILE")
		if certFile == "" || keyFile == "" {
			return nil, errors.New("启用 HTTPS_PORT 时必须配置 TLS_CERT_FILE 和 TLS_KEY_FILE")
		}
		tlsConfig, err := loadTLSConfig(certFile, keyFile, os.Getenv("TLS_CLIENT_CA_FILE"))
		if err != nil {
			return nil, err
		}
		listener, err := net.Listen("tcp", net.JoinHostPort(host, strconv.Itoa(port)))
		if err != nil {
			return nil, err
		}
		servers.httpsServer = &http.Server{Handler: handler, TLSConfig: tlsConfig, ReadHeaderTimeout: 10 * time.Second}
		go func() {
			if err := servers.httpsServer.ServeTLS(listener, "", ""); err != nil && err != http.ErrServerClosed {
				log.Printf("HTTPS 服务异常退出: %v", err)
			}
		}()
		log.Printf("HTTPS 服务启动在 %s", listener.Addr())
	}

	if port, enabled, err := optionalPort("GRPC_PORT"); err != nil {
		cleanup()
		return nil, err
	} else if enabled {
		listener, err := net.Listen("tcp", net.JoinHostPort(host, strconv.Itoa(port)))
		if err != nil {
			cleanup()
			return nil, err
		}
		servers.grpcServer = newGRPCServer(config)
		go func() {
			if err := servers.grpcServer.Serve(listener); err != nil {
				log.Printf("gRPC 服务异常退出: %v", err)
			}
		}()
		log.Printf("gRPC 服务启动在 %s", listener.Addr())
	}

	if port, enabled, err := optionalPort("TCP_PORT"); err != nil {
		cleanup()
		return nil, err
	} else if enabled {
		listener, err := net.Listen("tcp", net.JoinHostPort(host, strconv.Itoa(port)))
		if err != nil {
			cleanup()
			return nil, err
		}
		servers.tcpServer = newTCPEchoServer(listener)
		go servers.tcpServer.Serve()
		log.Printf("TCP Echo 服务启动在 %s", listener.Addr())
	}

	if port, enabled, err := optionalPort("UDP_PORT"); err != nil {
		cleanup()
		return nil, err
	} else if enabled {
		address, err := net.ResolveUDPAddr("udp", net.JoinHostPort(host, strconv.Itoa(port)))
		if err != nil {
			cleanup()
			return nil, err
		}
		connection, err := net.ListenUDP("udp", address)
		if err != nil {
			cleanup()
			return nil, err
		}
		servers.udpServer = newUDPEchoServer(connection)
		go servers.udpServer.Serve()
		log.Printf("UDP Echo 服务启动在 %s", connection.LocalAddr())
	}

	return servers, nil
}

func optionalPort(name string) (int, bool, error) {
	value := os.Getenv(name)
	if value == "" {
		return 0, false, nil
	}
	port, err := strconv.Atoi(value)
	if err != nil || port < 1 || port > 65535 {
		return 0, false, errors.New(name + " 必须是 1 到 65535 之间的整数")
	}
	return port, true, nil
}

func (servers *optionalProtocolServers) Shutdown(ctx context.Context) {
	var waitGroup sync.WaitGroup
	if servers.httpsServer != nil {
		waitGroup.Add(1)
		go func() {
			defer waitGroup.Done()
			_ = servers.httpsServer.Shutdown(ctx)
		}()
	}
	if servers.grpcServer != nil {
		waitGroup.Add(1)
		go func() {
			defer waitGroup.Done()
			done := make(chan struct{})
			go func() {
				servers.grpcServer.GracefulStop()
				close(done)
			}()
			select {
			case <-done:
			case <-ctx.Done():
				servers.grpcServer.Stop()
				<-done
			}
		}()
	}
	if servers.tcpServer != nil {
		_ = servers.tcpServer.Close()
	}
	if servers.udpServer != nil {
		_ = servers.udpServer.Close()
	}
	waitGroup.Wait()
}

func (servers *optionalProtocolServers) Close() {
	if servers.httpsServer != nil {
		_ = servers.httpsServer.Close()
	}
	if servers.grpcServer != nil {
		servers.grpcServer.Stop()
	}
	if servers.tcpServer != nil {
		_ = servers.tcpServer.Close()
	}
	if servers.udpServer != nil {
		_ = servers.udpServer.Close()
	}
}
