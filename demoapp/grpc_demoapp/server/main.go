package main

import (
	"context"
	pb "demoapp/data"
	"flag"
	"fmt"
	"google.golang.org/grpc"
	"google.golang.org/grpc/peer"
	"google.golang.org/grpc/reflection"
	"log"
	"net"
	"os"
	"time"
)

var (
	port     = flag.Int("port", 50051, "The server port")
	serverID = os.Getenv("HOSTNAME") // 从环境变量获取Pod名称作为服务器ID
)

type server struct {
	pb.UnimplementedDemoServer
}

func (s *server) Connect(req *pb.ConnectRequest, stream pb.Demo_ConnectServer) error {
	log.Printf("Client connected: %s", req.ClientId)

	for {
		select {
		case <-stream.Context().Done():
			log.Printf("Client disconnected: %s", req.ClientId)
			return nil
		default:
			// 获取服务端 IP（方式一：hostname -> lookup）
			hostname, err := os.Hostname()
			podIP := "unknown"
			if err == nil {
				if addrs, err := net.LookupHost(hostname); err == nil && len(addrs) > 0 {
					podIP = addrs[0]
				}
			}
			resp := &pb.ConnectResponse{
				ServerId:  serverID,
				Message:   fmt.Sprintf("Hello %s from server, ServerName: %s, ServerIP: %s", req.ClientId, hostname, podIP),
				Timestamp: time.Now().Unix(),
			}
			if err := stream.Send(resp); err != nil {
				return err
			}
			time.Sleep(2 * time.Second) // 每2秒发送一次消息
		}
	}
}

func (s *server) SayHello(ctx context.Context, req *pb.HelloRequest) (*pb.HelloResponse, error) {
	// 获取客户端 IP
	clientIP := "unknown"
	if p, ok := peer.FromContext(ctx); ok {
		if addr, ok := p.Addr.(*net.TCPAddr); ok {
			clientIP = addr.IP.String()
		} else {
			clientIP = p.Addr.String()
		}
	}

	// 获取服务端 IP（方式一：hostname -> lookup）
	hostname, err := os.Hostname()
	podIP := "unknown"
	if err == nil {
		if addrs, err := net.LookupHost(hostname); err == nil && len(addrs) > 0 {
			podIP = addrs[0]
		}
	}

	//message := fmt.Sprintf("Hello %s, ClientIP: %s, ServerName: %s, ServerIP: %s", in.Name, clientIP, hostname, podIP)
	message := fmt.Sprintf("grpc demoapp v1.0 !!, ClientIP: %s, ServerName: %s, ServerIP: %s", clientIP, hostname, podIP)

	log.Printf("SayHello called by client: %s", clientIP)

	resp := &pb.HelloResponse{
		Message: message,
	}
	return resp, nil
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
	pb.RegisterDemoServer(s, &server{})
	reflection.Register(s)

	log.Printf("server %s listening at %v", serverID, lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
