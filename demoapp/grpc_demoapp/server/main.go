package main

import (
	"context"
	pb "demoapp/data"
	"demoapp/server/services"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/peer"
	"google.golang.org/grpc/reflection"
)

var (
	port             = flag.Int("port", 50051, "The server port")
	serverID         = os.Getenv("HOSTNAME")
	version          = "1.2"
	startTime        = time.Now()
	connectedClients = make(map[string]bool)
	totalRequests    int64
	mu               sync.RWMutex
)

// 获取客户端信息的辅助函数
func getClientInfo(ctx context.Context) (clientIP, clientID string) {
	clientIP = "unknown"
	clientID = "unknown"
	if p, ok := peer.FromContext(ctx); ok {
		if addr, ok := p.Addr.(*net.TCPAddr); ok {
			clientIP = addr.IP.String()
		} else {
			clientIP = p.Addr.String()
		}
		clientID = p.Addr.String()
	}
	return
}

// 获取服务器信息的辅助函数
func getServerInfo() (hostname, podIP string) {
	hostname = "unknown"
	podIP = "unknown"
	var err error
	hostname, err = os.Hostname()
	if err == nil {
		if addrs, err := net.LookupHost(hostname); err == nil && len(addrs) > 0 {
			podIP = addrs[0]
		}
	}
	return
}

type server struct {
	pb.UnimplementedHelloWorldServer
	chatStreams map[string]pb.HelloWorld_ChatServer
	mu          sync.RWMutex
}

func newServer() *server {
	return &server{
		chatStreams: make(map[string]pb.HelloWorld_ChatServer),
	}
}

func (s *server) Connect(req *pb.ConnectRequest, stream pb.HelloWorld_ConnectServer) error {
	clientIP, _ := getClientInfo(stream.Context())
	hostname, podIP := getServerInfo()
	startTime := time.Now()

	log.Printf("[Connect] 开始处理请求 - ClientIP: %s, ClientID: %s, ServerName: %s, ServerIP: %s",
		clientIP, req.ClientId, hostname, podIP)

	mu.Lock()
	connectedClients[req.ClientId] = true
	totalRequests++
	mu.Unlock()

	defer func() {
		mu.Lock()
		delete(connectedClients, req.ClientId)
		mu.Unlock()
		log.Printf("[Connect] 结束处理请求 - ClientID: %s, 处理时长: %v", req.ClientId, time.Since(startTime))
	}()

	for {
		select {
		case <-stream.Context().Done():
			log.Printf("[Connect] 客户端断开连接 - ClientID: %s, 总连接时长: %v", req.ClientId, time.Since(startTime))
			return nil
		default:
			hostname, podIP := getServerInfo()
			resp := &pb.ConnectResponse{
				ServerId:  serverID,
				Message:   fmt.Sprintf("Hello %s from server, ServerName: %s, ServerIP: %s", req.ClientId, hostname, podIP),
				Timestamp: time.Now().Unix(),
			}
			if err := stream.Send(resp); err != nil {
				log.Printf("[Connect] 发送消息失败 - ClientID: %s, Error: %v", req.ClientId, err)
				return err
			}
			log.Printf("[Connect] 发送消息成功 - ClientID: %s, Message: %s", req.ClientId, resp.Message)
			time.Sleep(2 * time.Second)
		}
	}
}

func (s *server) SayHello(ctx context.Context, req *pb.HelloRequest) (*pb.HelloResponse, error) {
	startTime := time.Now()
	clientIP, _ := getClientInfo(ctx)
	hostname, podIP := getServerInfo()

	log.Printf("[SayHello] 收到请求 - ClientIP: %s, Name: %s", clientIP, req.Name)

	mu.Lock()
	totalRequests++
	mu.Unlock()

	message := fmt.Sprintf("grpc demoapp v%s, ClientIP: %s, ServerName: %s, ServerIP: %s", version, clientIP, hostname, podIP)
	response := &pb.HelloResponse{Message: message}

	log.Printf("[SayHello] 请求处理完成 - ClientIP: %s, Response: %s, 处理时长: %v",
		clientIP, response.Message, time.Since(startTime))

	return response, nil
}

func (s *server) UploadData(stream pb.HelloWorld_UploadDataServer) error {
	startTime := time.Now()
	clientIP, _ := getClientInfo(stream.Context())
	var totalBytes int64
	var chunksReceived int32

	log.Printf("[UploadData] 开始接收数据流 - ClientIP: %s", clientIP)

	for {
		chunk, err := stream.Recv()
		if err == io.EOF {
			response := &pb.UploadResponse{
				TotalBytes:     totalBytes,
				ChunksReceived: chunksReceived,
				Message:        "Upload completed successfully",
			}
			log.Printf("[UploadData] 数据流接收完成 - ClientIP: %s, TotalBytes: %d, Chunks: %d, 总处理时长: %v",
				clientIP, totalBytes, chunksReceived, time.Since(startTime))
			return stream.SendAndClose(response)
		}
		if err != nil {
			log.Printf("[UploadData] 数据流接收错误 - ClientIP: %s, Error: %v", clientIP, err)
			return err
		}

		totalBytes += int64(len(chunk.Data))
		chunksReceived++
		log.Printf("[UploadData] 接收数据块 - ClientIP: %s, ClientID: %s, ChunkSize: %d bytes, TotalReceived: %d bytes",
			clientIP, chunk.ClientId, len(chunk.Data), totalBytes)
	}
}

func (s *server) Chat(stream pb.HelloWorld_ChatServer) error {
	startTime := time.Now()
	clientIP, clientID := getClientInfo(stream.Context())

	log.Printf("[Chat] 新客户端连接 - ClientIP: %s, ClientID: %s", clientIP, clientID)

	s.mu.Lock()
	s.chatStreams[clientID] = stream
	currentClients := len(s.chatStreams)
	s.mu.Unlock()

	log.Printf("[Chat] 当前连接数: %d", currentClients)

	defer func() {
		s.mu.Lock()
		delete(s.chatStreams, clientID)
		log.Printf("[Chat] 客户端断开连接 - ClientIP: %s, ClientID: %s, 会话时长: %v",
			clientIP, clientID, time.Since(startTime))
		s.mu.Unlock()
	}()

	for {
		in, err := stream.Recv()
		if err == io.EOF {
			log.Printf("[Chat] 客户端结束发送 - ClientIP: %s, ClientID: %s", clientIP, clientID)
			return nil
		}
		if err != nil {
			log.Printf("[Chat] 接收消息错误 - ClientIP: %s, ClientID: %s, Error: %v",
				clientIP, clientID, err)
			return err
		}

		log.Printf("[Chat] 收到消息 - From: %s, Content: %s, Timestamp: %d",
			in.Sender, in.Content, in.Timestamp)

		msg := &pb.ChatMessage{
			Sender:    in.Sender,
			Content:   in.Content,
			Timestamp: time.Now().Unix(),
		}

		s.mu.RLock()
		for id, clientStream := range s.chatStreams {
			if err := clientStream.Send(msg); err != nil {
				log.Printf("[Chat] 发送消息失败 - To: %s, Error: %v", id, err)
			} else {
				log.Printf("[Chat] 发送消息成功 - To: %s, Content: %s", id, msg.Content)
			}
		}
		s.mu.RUnlock()
	}
}

func (s *server) GetServerStats(ctx context.Context, req *pb.StatsRequest) (*pb.StatsResponse, error) {
	startTime := time.Now()
	clientIP, _ := getClientInfo(ctx)

	log.Printf("[GetServerStats] 收到请求 - ClientIP: %s, ClientID: %s", clientIP, req.ClientId)

	mu.RLock()
	clientCount := len(connectedClients)
	requests := totalRequests
	mu.RUnlock()

	response := &pb.StatsResponse{
		ServerId:         serverID,
		Uptime:           int64(time.Since(startTime).Seconds()),
		ConnectedClients: int32(clientCount),
		TotalRequests:    requests,
		Version:          version,
	}

	responseJSON, _ := json.Marshal(response)
	log.Printf("[GetServerStats] 请求处理完成 - ClientIP: %s, Response: %s, 处理时长: %v",
		clientIP, string(responseJSON), time.Since(startTime))

	return response, nil
}

func main() {
	flag.Parse()
	if serverID == "" {
		serverID = fmt.Sprintf("standalone-%d", *port)
	}

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	s := grpc.NewServer()

	// 注册HelloWorld服务
	pb.RegisterHelloWorldServer(s, newServer())

	// 注册User服务
	userServer := services.NewUserServer()
	pb.RegisterUserServer(s, userServer)

	// 注册反射服务
	reflection.Register(s)

	log.Printf("server %s listening at %v", serverID, lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
