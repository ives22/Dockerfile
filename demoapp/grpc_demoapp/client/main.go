package main

import (
	"context"
	"demoapp/client/services"
	pb "demoapp/data"
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// 定义测试方法映射
var testMethods = map[string]func(context.Context, interface{}, string){
	"hello": func(ctx context.Context, client interface{}, clientID string) {
		runHello(ctx, client.(pb.HelloWorldClient), clientID)
	},
	"connect": func(ctx context.Context, client interface{}, clientID string) {
		runConnect(ctx, client.(pb.HelloWorldClient), clientID)
	},
	"upload": func(ctx context.Context, client interface{}, clientID string) {
		runUpload(ctx, client.(pb.HelloWorldClient), clientID)
	},
	"chat": func(ctx context.Context, client interface{}, clientID string) {
		runChat(ctx, client.(pb.HelloWorldClient), clientID)
	},
	"stats": func(ctx context.Context, client interface{}, clientID string) {
		runStats(ctx, client.(pb.HelloWorldClient), clientID)
	},
	"registerv1": func(ctx context.Context, client interface{}, _ string) {
		services.TestRegisterV1(ctx, client.(pb.UserClient))
	},
	"registerv2": func(ctx context.Context, client interface{}, _ string) {
		services.TestRegisterV2(ctx, client.(pb.UserClient))
	},
	"loginv1": func(ctx context.Context, client interface{}, _ string) {
		services.TestLoginV1(ctx, client.(pb.UserClient))
	},
	"loginv2": func(ctx context.Context, client interface{}, _ string) {
		services.TestLoginV2(ctx, client.(pb.UserClient))
	},
	"updatesv1": func(ctx context.Context, client interface{}, _ string) {
		services.TestUserUpdatesV1(ctx, client.(pb.UserClient))
	},
	"updatesv2": func(ctx context.Context, client interface{}, _ string) {
		services.TestUserUpdatesV2(ctx, client.(pb.UserClient))
	},
}

func printUsage() {
	fmt.Println("用法: go run client/main.go [测试方法名]")
	fmt.Println("\n可用的测试方法:")
	fmt.Println("HelloWorld服务:")
	fmt.Println("  hello      - 测试SayHello接口")
	fmt.Println("  connect    - 测试Connect接口（服务器流）")
	fmt.Println("  upload     - 测试UploadData接口（客户端流）")
	fmt.Println("  chat       - 测试Chat接口（双向流）")
	fmt.Println("  stats      - 测试GetServerStats接口")
	fmt.Println("\nUser服务:")
	fmt.Println("  registerv1 - 测试RegisterV1接口")
	fmt.Println("  registerv2 - 测试RegisterV2接口")
	fmt.Println("  loginv1    - 测试LoginV1接口")
	fmt.Println("  loginv2    - 测试LoginV2接口")
	fmt.Println("  updatesv1  - 测试GetUserUpdatesV1接口")
	fmt.Println("  updatesv2  - 测试GetUserUpdatesV2接口")
	fmt.Println("\n不带参数则执行所有测试")
}

func main() {
	// 检查是否需要显示帮助信息
	if len(os.Args) > 1 && (os.Args[1] == "-h" || os.Args[1] == "--help") {
		printUsage()
		return
	}

	// 从环境变量获取服务器地址
	serverAddr := os.Getenv("SERVER_ADDR")
	if serverAddr == "" {
		serverAddr = "localhost:50051"
	}

	// 生成客户端ID
	clientID := ""
	hostname, err := os.Hostname()
	if err == nil {
		clientID = hostname
	} else {
		clientID = fmt.Sprintf("client-%d", time.Now().Unix())
	}

	// 建立连接
	conn, err := grpc.Dial(serverAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()

	// 创建上下文
	ctx := context.Background()

	// 创建服务客户端
	helloWorldClient := pb.NewHelloWorldClient(conn)
	userClient := pb.NewUserClient(conn)

	// 如果指定了测试方法
	if len(os.Args) > 1 {
		methodName := strings.ToLower(os.Args[1])
		if testFunc, exists := testMethods[methodName]; exists {
			log.Printf("执行测试: %s\n", methodName)
			if strings.HasPrefix(methodName, "register") ||
				strings.HasPrefix(methodName, "login") ||
				strings.HasPrefix(methodName, "updates") {
				testFunc(ctx, userClient, clientID)
			} else {
				testFunc(ctx, helloWorldClient, clientID)
			}
		} else {
			fmt.Printf("未知的测试方法: %s\n", methodName)
			printUsage()
		}
		return
	}

	// 执行所有测试
	log.Println("执行所有测试...")

	// 测试User服务
	log.Println("\n测试 User 服务:")
	services.RunUserTest(ctx, userClient)

	// 测试HelloWorld服务
	log.Println("\n测试 HelloWorld 服务:")
	runHello(ctx, helloWorldClient, clientID)
	log.Println("Stats test:")
	runStats(ctx, helloWorldClient, clientID)
	log.Println("Upload test:")
	runUpload(ctx, helloWorldClient, clientID)
	log.Println("Chat test:")
	runChat(ctx, helloWorldClient, clientID)
	log.Println("Connect test:")
	runConnect(ctx, helloWorldClient, clientID)
}

// 测试SayHello接口
func runHello(ctx context.Context, client pb.HelloWorldClient, clientID string) {
	resp, err := client.SayHello(ctx, &pb.HelloRequest{Name: clientID})
	if err != nil {
		log.Printf("SayHello failed: %v", err)
		return
	}
	log.Printf("SayHello Response: %s", resp.Message)
}

// 测试Connect接口（服务器流）
func runConnect(ctx context.Context, client pb.HelloWorldClient, clientID string) {
	// 创建一个带有超时的上下文
	timeoutCtx, cancel := context.WithTimeout(ctx, 10*time.Second)
	defer cancel()

	stream, err := client.Connect(timeoutCtx, &pb.ConnectRequest{ClientId: clientID})
	if err != nil {
		log.Printf("Connect failed: %v", err)
		return
	}

	// 使用channel来控制接收
	done := make(chan bool)
	go func() {
		for {
			resp, err := stream.Recv()
			if err == io.EOF {
				done <- true
				return
			}
			if err != nil {
				log.Printf("Connect stream receive failed: %v", err)
				done <- true
				return
			}
			log.Printf("Connect Response: server_id=%s, message=%s, timestamp=%d",
				resp.ServerId, resp.Message, resp.Timestamp)
		}
	}()

	// 等待超时或接收完成
	select {
	case <-timeoutCtx.Done():
		log.Printf("Connect test timeout after 10 seconds")
		return
	case <-done:
		log.Printf("Connect test completed")
		return
	}
}

// 测试UploadData接口（客户端流）
func runUpload(ctx context.Context, client pb.HelloWorldClient, clientID string) {
	stream, err := client.UploadData(ctx)
	if err != nil {
		log.Printf("UploadData failed: %v", err)
		return
	}

	// 模拟上传数据
	for i := 0; i < 5; i++ {
		data := []byte(fmt.Sprintf("test data %d", i))
		if err := stream.Send(&pb.UploadRequest{
			ClientId:  clientID,
			Data:      data,
			Timestamp: time.Now().Unix(),
		}); err != nil {
			log.Printf("UploadData send failed: %v", err)
			return
		}
		time.Sleep(time.Second)
	}

	resp, err := stream.CloseAndRecv()
	if err != nil {
		log.Printf("UploadData close failed: %v", err)
		return
	}
	log.Printf("UploadData Response: total_bytes=%d, chunks=%d, message=%s",
		resp.TotalBytes, resp.ChunksReceived, resp.Message)
}

// 测试Chat接口（双向流）
func runChat(ctx context.Context, client pb.HelloWorldClient, clientID string) {
	stream, err := client.Chat(ctx)
	if err != nil {
		log.Printf("Chat failed: %v", err)
		return
	}

	// 创建等待组
	var wg sync.WaitGroup
	wg.Add(2)

	// 发送消息的goroutine
	go func() {
		defer wg.Done()
		for i := 0; i < 5; i++ {
			msg := fmt.Sprintf("Message %d from %s", i, clientID)
			if err := stream.Send(&pb.ChatMessage{
				Sender:    clientID,
				Content:   msg,
				Timestamp: time.Now().Unix(),
			}); err != nil {
				log.Printf("Chat send failed: %v", err)
				return
			}
			time.Sleep(time.Second)
		}
	}()

	// 接收消息的goroutine
	go func() {
		defer wg.Done()
		for {
			msg, err := stream.Recv()
			if err == io.EOF {
				return
			}
			if err != nil {
				log.Printf("Chat receive failed: %v", err)
				return
			}
			log.Printf("Chat Message: sender=%s, content=%s, timestamp=%d",
				msg.Sender, msg.Content, msg.Timestamp)
		}
	}()

	// 等待发送和接收完成
	wg.Wait()
	if err := stream.CloseSend(); err != nil {
		log.Printf("Chat close failed: %v", err)
	}
}

// 测试GetServerStats接口
func runStats(ctx context.Context, client pb.HelloWorldClient, clientID string) {
	resp, err := client.GetServerStats(ctx, &pb.StatsRequest{ClientId: clientID})
	if err != nil {
		log.Printf("GetServerStats failed: %v", err)
		return
	}
	log.Printf("GetServerStats Response: server_id=%s, uptime=%d, connected=%d, requests=%d, version=%s",
		resp.ServerId, resp.Uptime, resp.ConnectedClients, resp.TotalRequests, resp.Version)
}
