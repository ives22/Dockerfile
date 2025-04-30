package main

import (
	"context"
	pb "demoapp/data"
	"fmt"
	"io"
	"log"
	"time"
)

// 测试用户注册V1
func runRegisterV1(ctx context.Context, client pb.UserClient) {
	resp, err := client.RegisterV1(ctx, &pb.RegisterRequest{
		Username: "testuser",
		Password: "password123",
		Email:    "test@example.com",
	})
	if err != nil {
		log.Fatalf("注册失败: %v", err)
	}
	log.Printf("注册结果: 成功=%v, 用户ID=%s, 消息=%s",
		resp.Success, resp.UserId, resp.Message)
}

// 测试用户注册V2
func runRegisterV2(ctx context.Context, client pb.UserClient) {
	resp, err := client.RegisterV2(ctx, &pb.RegisterRequestV2{
		Username: "testuser2",
		Password: "password123",
		Email:    "test2@example.com",
		Phone:    "13800138000",
		Nickname: "测试用户2",
		Metadata: map[string]string{
			"age":      "25",
			"location": "Beijing",
		},
	})
	if err != nil {
		log.Fatalf("注册失败: %v", err)
	}
	log.Printf("注册结果: 成功=%v, 用户ID=%s, 消息=%s",
		resp.Success, resp.UserId, resp.Message)
}

// 测试用户登录V1
func runLoginV1(ctx context.Context, client pb.UserClient) {
	resp, err := client.LoginV1(ctx, &pb.LoginRequest{
		Username: "testuser",
		Password: "password123",
	})
	if err != nil {
		log.Fatalf("登录失败: %v", err)
	}
	log.Printf("登录结果: 成功=%v, Token=%s, 消息=%s\n用户信息: %+v",
		resp.Success, resp.Token, resp.Message, resp.UserInfo)
}

// 测试用户登录V2
func runLoginV2(ctx context.Context, client pb.UserClient) {
	// 测试不同的登录方式
	loginTypes := []struct {
		loginType  pb.LoginRequestV2_LoginType
		identifier string
		credential string
		typeDesc   string
	}{
		{pb.LoginRequestV2_USERNAME, "testuser2", "password123", "用户名"},
		{pb.LoginRequestV2_EMAIL, "test2@example.com", "password123", "邮箱"},
		{pb.LoginRequestV2_PHONE, "13800138000", "password123", "手机号"},
	}

	for _, lt := range loginTypes {
		resp, err := client.LoginV2(ctx, &pb.LoginRequestV2{
			LoginType:  lt.loginType,
			Identifier: lt.identifier,
			Credential: lt.credential,
		})
		if err != nil {
			log.Printf("%s登录失败: %v", lt.typeDesc, err)
			continue
		}
		log.Printf("%s登录结果: 成功=%v, Token=%s, 消息=%s\n用户信息: %+v",
			lt.typeDesc, resp.Success, resp.Token, resp.Message, resp.UserInfo)
	}
}

// 测试用户状态更新V1
func runUserUpdatesV1(ctx context.Context, client pb.UserClient, userID string) {
	stream, err := client.GetUserUpdatesV1(ctx, &pb.UserRequest{
		UserId: userID,
	})
	if err != nil {
		log.Fatalf("获取用户更新失败: %v", err)
	}

	for {
		update, err := stream.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalf("接收更新失败: %v", err)
		}
		log.Printf("收到用户状态更新V1: 用户ID=%s, 状态=%s, 时间=%v",
			update.UserId, update.Status,
			time.Unix(update.Timestamp, 0).Format("2006-01-02 15:04:05"))
	}
}

// 测试用户状态更新V2
func runUserUpdatesV2(ctx context.Context, client pb.UserClient, userID string) {
	stream, err := client.GetUserUpdatesV2(ctx, &pb.UserRequest{
		UserId: userID,
	})
	if err != nil {
		log.Fatalf("获取用户更新失败: %v", err)
	}

	for {
		update, err := stream.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalf("接收更新失败: %v", err)
		}
		log.Printf("收到用户状态更新V2:\n"+
			"用户ID: %s\n"+
			"状态: %s\n"+
			"时间: %v\n"+
			"活动会话: %v\n"+
			"自定义状态: %v\n"+
			"位置: %+v\n"+
			"设备信息: %+v",
			update.UserId,
			update.Status,
			time.Unix(update.Timestamp, 0).Format("2006-01-02 15:04:05"),
			update.ActiveSessions,
			update.CustomStatus,
			update.LastLocation,
			update.DeviceInfo)
	}
}

// 运行User服务测试
func runUserTest(ctx context.Context, client pb.UserClient) {
	fmt.Println("\n=== 测试用户注册V1 ===")
	runRegisterV1(ctx, client)

	fmt.Println("\n=== 测试用户注册V2 ===")
	runRegisterV2(ctx, client)

	fmt.Println("\n=== 测试用户登录V1 ===")
	runLoginV1(ctx, client)

	fmt.Println("\n=== 测试用户登录V2 ===")
	runLoginV2(ctx, client)

	// 使用注册的用户ID测试状态更新
	userID := "user_1" // 这是第一个注册的用户ID

	fmt.Println("\n=== 测试用户状态更新V1 ===")
	ctx1, cancel1 := context.WithTimeout(ctx, 30*time.Second)
	defer cancel1()
	runUserUpdatesV1(ctx1, client, userID)

	fmt.Println("\n=== 测试用户状态更新V2 ===")
	ctx2, cancel2 := context.WithTimeout(ctx, 30*time.Second)
	defer cancel2()
	runUserUpdatesV2(ctx2, client, userID)
}
