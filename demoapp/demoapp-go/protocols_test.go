package main

import (
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/json"
	"encoding/pem"
	"io"
	"math/big"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
	"time"

	demov1 "demoapp/proto/demo/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/health/grpc_health_v1"
	"google.golang.org/grpc/test/bufconn"
)

func TestGRPCEchoAndHealth(t *testing.T) {
	listener := bufconn.Listen(1024 * 1024)
	server := newGRPCServer(diagnosticConfig{Pod: "demoapp-abc", PodIP: "10.244.1.20", Version: "v3"})
	go func() { _ = server.Serve(listener) }()
	defer server.Stop()

	connection, err := grpc.NewClient("passthrough:///bufnet",
		grpc.WithContextDialer(func(context.Context, string) (net.Conn, error) { return listener.Dial() }),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		t.Fatalf("创建 gRPC 客户端失败: %v", err)
	}
	defer connection.Close()

	echoResponse, err := demov1.NewDemoServiceClient(connection).Echo(context.Background(), &demov1.EchoRequest{Message: "hello"})
	if err != nil {
		t.Fatalf("gRPC Echo 失败: %v", err)
	}
	if echoResponse.GetMessage() != "hello" || echoResponse.GetPod() != "demoapp-abc" || echoResponse.GetPodIp() != "10.244.1.20" || echoResponse.GetVersion() != "v3" {
		t.Fatalf("gRPC Echo 响应不正确: %#v", echoResponse)
	}

	healthResponse, err := grpc_health_v1.NewHealthClient(connection).Check(context.Background(), &grpc_health_v1.HealthCheckRequest{})
	if err != nil || healthResponse.GetStatus() != grpc_health_v1.HealthCheckResponse_SERVING {
		t.Fatalf("gRPC Health 不正确: response=%#v err=%v", healthResponse, err)
	}
}

func TestTCPEchoServer(t *testing.T) {
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("创建 TCP 监听失败: %v", err)
	}
	server := newTCPEchoServer(listener)
	defer server.Close()
	go server.Serve()

	connection, err := net.DialTimeout("tcp", listener.Addr().String(), time.Second)
	if err != nil {
		t.Fatalf("连接 TCP Echo 失败: %v", err)
	}
	defer connection.Close()
	if _, err := connection.Write([]byte("hello tcp")); err != nil {
		t.Fatalf("写入 TCP Echo 失败: %v", err)
	}
	buffer := make([]byte, len("hello tcp"))
	if _, err := io.ReadFull(connection, buffer); err != nil {
		t.Fatalf("读取 TCP Echo 失败: %v", err)
	}
	if string(buffer) != "hello tcp" {
		t.Fatalf("TCP Echo 响应不正确: %q", buffer)
	}
}

func TestTCPEchoServerCloseClosesActiveConnections(t *testing.T) {
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("创建 TCP 监听失败: %v", err)
	}
	server := newTCPEchoServer(listener)
	go server.Serve()

	connection, err := net.DialTimeout("tcp", listener.Addr().String(), time.Second)
	if err != nil {
		t.Fatalf("连接 TCP Echo 失败: %v", err)
	}
	defer connection.Close()
	if _, err := connection.Write([]byte("ready")); err != nil {
		t.Fatalf("写入 TCP Echo 失败: %v", err)
	}
	buffer := make([]byte, len("ready"))
	if _, err := io.ReadFull(connection, buffer); err != nil {
		t.Fatalf("读取 TCP Echo 失败: %v", err)
	}

	if err := server.Close(); err != nil {
		t.Fatalf("关闭 TCP Echo 失败: %v", err)
	}
	_ = connection.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
	if _, err := connection.Read(make([]byte, 1)); err == nil {
		t.Fatal("TCP Echo 关闭后，活动连接仍可继续读取")
	} else if networkError, ok := err.(net.Error); ok && networkError.Timeout() {
		t.Fatal("TCP Echo 关闭后，活动连接未被及时关闭")
	}
}

func TestUDPEchoServer(t *testing.T) {
	connection, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("创建 UDP 监听失败: %v", err)
	}
	server := newUDPEchoServer(connection)
	defer server.Close()
	go server.Serve()

	client, err := net.DialUDP("udp", nil, connection.LocalAddr().(*net.UDPAddr))
	if err != nil {
		t.Fatalf("连接 UDP Echo 失败: %v", err)
	}
	defer client.Close()
	_ = client.SetDeadline(time.Now().Add(time.Second))
	if _, err := client.Write([]byte("hello udp")); err != nil {
		t.Fatalf("写入 UDP Echo 失败: %v", err)
	}
	buffer := make([]byte, 64)
	size, err := client.Read(buffer)
	if err != nil {
		t.Fatalf("读取 UDP Echo 失败: %v", err)
	}
	if string(buffer[:size]) != "hello udp" {
		t.Fatalf("UDP Echo 响应不正确: %q", buffer[:size])
	}
}

func TestStressEndpointRequiresConfiguredToken(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/stress/cpu?milliseconds=1", nil))
	if recorder.Code != http.StatusNotFound {
		t.Fatalf("未配置 Token 时压力接口应为 404，实际为 %d", recorder.Code)
	}
}

func TestStressEndpointsValidateTokenAndLimits(t *testing.T) {
	config := diagnosticConfig{StressToken: "secret"}
	router := newTestRouter(config)

	unauthorized := httptest.NewRecorder()
	router.ServeHTTP(unauthorized, httptest.NewRequest(http.MethodGet, "/stress/cpu?milliseconds=1", nil))
	if unauthorized.Code != http.StatusUnauthorized {
		t.Fatalf("缺少 Token 期望 401，实际为 %d", unauthorized.Code)
	}

	authorizedRequest := httptest.NewRequest(http.MethodGet, "/stress/cpu?milliseconds=1", nil)
	authorizedRequest.Header.Set("X-Stress-Token", "secret")
	authorized := httptest.NewRecorder()
	router.ServeHTTP(authorized, authorizedRequest)
	if authorized.Code != http.StatusOK {
		t.Fatalf("合法 CPU 压力请求期望 200，实际为 %d", authorized.Code)
	}

	overLimitRequest := httptest.NewRequest(http.MethodGet, "/stress/memory?megabytes=257&holdSeconds=0", nil)
	overLimitRequest.Header.Set("X-Stress-Token", "secret")
	overLimit := httptest.NewRecorder()
	router.ServeHTTP(overLimit, overLimitRequest)
	if overLimit.Code != http.StatusBadRequest {
		t.Fatalf("超限内存请求期望 400，实际为 %d", overLimit.Code)
	}
}

func TestLoadTLSConfigSupportsServerTLSAndMTLS(t *testing.T) {
	tempDir := t.TempDir()
	caCert, caKey, caPEM := createTestCA(t)
	serverCertPath, serverKeyPath := createSignedCertificate(t, tempDir, "server", caCert, caKey, false)
	clientCertPath, clientKeyPath := createSignedCertificate(t, tempDir, "client", caCert, caKey, true)
	caPath := filepath.Join(tempDir, "ca.crt")
	if err := os.WriteFile(caPath, caPEM, 0o600); err != nil {
		t.Fatalf("写入 CA 失败: %v", err)
	}

	tlsConfig, err := loadTLSConfig(serverCertPath, serverKeyPath, caPath)
	if err != nil {
		t.Fatalf("加载 TLS 配置失败: %v", err)
	}
	if tlsConfig.ClientAuth != tls.RequireAndVerifyClientCert {
		t.Fatalf("配置 CA 后应要求并校验客户端证书: %v", tlsConfig.ClientAuth)
	}
	if _, err := tls.LoadX509KeyPair(clientCertPath, clientKeyPath); err != nil {
		t.Fatalf("客户端证书无效: %v", err)
	}
}

func TestMTLSServerRejectsAnonymousClientAndAcceptsClientCertificate(t *testing.T) {
	tempDir := t.TempDir()
	caCert, caKey, caPEM := createTestCA(t)
	serverCertPath, serverKeyPath := createSignedCertificate(t, tempDir, "server", caCert, caKey, false)
	clientCertPath, clientKeyPath := createSignedCertificate(t, tempDir, "client", caCert, caKey, true)
	caPath := filepath.Join(tempDir, "ca.crt")
	if err := os.WriteFile(caPath, caPEM, 0o600); err != nil {
		t.Fatalf("写入 CA 失败: %v", err)
	}

	tlsConfig, err := loadTLSConfig(serverCertPath, serverKeyPath, caPath)
	if err != nil {
		t.Fatalf("加载 mTLS 配置失败: %v", err)
	}
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("创建 mTLS 监听失败: %v", err)
	}
	server := &http.Server{Handler: newTestRouter(diagnosticConfig{Pod: "mtls-pod"}), TLSConfig: tlsConfig}
	go func() { _ = server.ServeTLS(listener, "", "") }()
	defer server.Close()

	rootCAs := x509.NewCertPool()
	if !rootCAs.AppendCertsFromPEM(caPEM) {
		t.Fatal("测试 CA 无法加入根证书池")
	}
	serverURL := "https://" + listener.Addr().String() + "/request"
	anonymousClient := &http.Client{
		Timeout:   2 * time.Second,
		Transport: &http.Transport{TLSClientConfig: &tls.Config{RootCAs: rootCAs, MinVersion: tls.VersionTLS12}, ForceAttemptHTTP2: true},
	}
	if response, err := anonymousClient.Get(serverURL); err == nil {
		response.Body.Close()
		t.Fatal("未携带客户端证书的 mTLS 请求应握手失败")
	}

	clientCertificate, err := tls.LoadX509KeyPair(clientCertPath, clientKeyPath)
	if err != nil {
		t.Fatalf("加载客户端证书失败: %v", err)
	}
	authenticatedClient := &http.Client{
		Timeout: 2 * time.Second,
		Transport: &http.Transport{TLSClientConfig: &tls.Config{
			RootCAs:      rootCAs,
			Certificates: []tls.Certificate{clientCertificate},
			MinVersion:   tls.VersionTLS12,
		}, ForceAttemptHTTP2: true},
	}
	response, err := authenticatedClient.Get(serverURL)
	if err != nil {
		t.Fatalf("携带客户端证书的 mTLS 请求失败: %v", err)
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		t.Fatalf("mTLS 请求期望状态码 200，实际为 %d", response.StatusCode)
	}
	if response.ProtoMajor != 2 {
		t.Fatalf("HTTPS 应通过 ALPN 支持 HTTP/2，实际协议为 %s", response.Proto)
	}
	var body requestInfoResponse
	if err := json.NewDecoder(response.Body).Decode(&body); err != nil {
		t.Fatalf("解析 mTLS 响应失败: %v", err)
	}
	if body.Path != "/request" || body.Pod != "mtls-pod" {
		t.Fatalf("mTLS 响应不正确: %#v", body)
	}
}

func createTestCA(t *testing.T) (*x509.Certificate, *rsa.PrivateKey, []byte) {
	t.Helper()
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("生成 CA 私钥失败: %v", err)
	}
	template := &x509.Certificate{SerialNumber: big.NewInt(1), Subject: pkix.Name{CommonName: "demo-ca"}, NotBefore: time.Now().Add(-time.Hour), NotAfter: time.Now().Add(time.Hour), IsCA: true, BasicConstraintsValid: true, KeyUsage: x509.KeyUsageCertSign | x509.KeyUsageDigitalSignature}
	der, err := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if err != nil {
		t.Fatalf("生成 CA 证书失败: %v", err)
	}
	template.Raw = der
	return template, key, pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der})
}

func createSignedCertificate(t *testing.T, directory, name string, ca *x509.Certificate, caKey *rsa.PrivateKey, client bool) (string, string) {
	t.Helper()
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("生成 %s 私钥失败: %v", name, err)
	}
	usage := []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth}
	if client {
		usage = []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth}
	}
	template := &x509.Certificate{SerialNumber: big.NewInt(time.Now().UnixNano()), Subject: pkix.Name{CommonName: name}, DNSNames: []string{"localhost"}, IPAddresses: []net.IP{net.ParseIP("127.0.0.1")}, NotBefore: time.Now().Add(-time.Hour), NotAfter: time.Now().Add(time.Hour), KeyUsage: x509.KeyUsageDigitalSignature | x509.KeyUsageKeyEncipherment, ExtKeyUsage: usage}
	der, err := x509.CreateCertificate(rand.Reader, template, ca, &key.PublicKey, caKey)
	if err != nil {
		t.Fatalf("生成 %s 证书失败: %v", name, err)
	}
	certPath := filepath.Join(directory, name+".crt")
	keyPath := filepath.Join(directory, name+".key")
	if err := os.WriteFile(certPath, pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der}), 0o600); err != nil {
		t.Fatalf("写入 %s 证书失败: %v", name, err)
	}
	if err := os.WriteFile(keyPath, pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)}), 0o600); err != nil {
		t.Fatalf("写入 %s 私钥失败: %v", name, err)
	}
	return certPath, keyPath
}
