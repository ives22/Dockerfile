package services

import (
	"context"
	pb "demoapp/data"
	"io"
	"log"
	"time"
)

// RunUserTest 运行用户服务测试
func RunUserTest(ctx context.Context, client pb.UserClient) {
	// 测试V1版本注册
	TestRegisterV1(ctx, client)

	// 测试V2版本注册
	TestRegisterV2(ctx, client)

	// 测试V1版本登录
	TestLoginV1(ctx, client)

	// 测试V2版本登录
	TestLoginV2(ctx, client)

	// 测试V1版本状态更新
	TestUserUpdatesV1(ctx, client)

	// 测试V2版本状态更新
	TestUserUpdatesV2(ctx, client)
}

// TestRegisterV1 测试V1版本注册接口
func TestRegisterV1(ctx context.Context, client pb.UserClient) {
	log.Println("测试 RegisterV1 接口...")
	resp, err := client.RegisterV1(ctx, &pb.RegisterRequest{
		Username: "testuser",
		Password: "password123",
		Email:    "test@example.com",
	})
	if err != nil {
		log.Printf("RegisterV1 失败: %v\n", err)
		return
	}
	log.Printf("RegisterV1 响应: success=%v, message=%s, userId=%s\n",
		resp.Success, resp.Message, resp.UserId)
}

// TestRegisterV2 测试V2版本注册接口
func TestRegisterV2(ctx context.Context, client pb.UserClient) {
	log.Println("测试 RegisterV2 接口...")
	resp, err := client.RegisterV2(ctx, &pb.RegisterRequestV2{
		Username: "testuser2",
		Password: "password123",
		Email:    "test2@example.com",
		Phone:    "1234567890",
		Nickname: "测试用户2",
		Metadata: map[string]string{
			"region": "上海",
			"age":    "25",
		},
	})
	if err != nil {
		log.Printf("RegisterV2 失败: %v\n", err)
		return
	}
	log.Printf("RegisterV2 响应: success=%v, message=%s, userId=%s\n",
		resp.Success, resp.Message, resp.UserId)
}

// TestLoginV1 测试V1版本登录接口
func TestLoginV1(ctx context.Context, client pb.UserClient) {
	log.Println("测试 LoginV1 接口...")
	resp, err := client.LoginV1(ctx, &pb.LoginRequest{
		Username: "testuser",
		Password: "password123",
	})
	if err != nil {
		log.Printf("LoginV1 失败: %v\n", err)
		return
	}
	log.Printf("LoginV1 响应: success=%v, message=%s, token=%s\n",
		resp.Success, resp.Message, resp.Token)
	if resp.UserInfo != nil {
		log.Printf("用户信息: id=%s, username=%s, email=%s\n",
			resp.UserInfo.UserId, resp.UserInfo.Username, resp.UserInfo.Email)
	}
}

// TestLoginV2 测试V2版本登录接口
func TestLoginV2(ctx context.Context, client pb.UserClient) {
	log.Println("测试 LoginV2 接口...")
	resp, err := client.LoginV2(ctx, &pb.LoginRequestV2{
		Identifier: "test2@example.com",
		Credential: "password123",
		LoginType:  pb.LoginRequestV2_EMAIL,
	})
	if err != nil {
		log.Printf("LoginV2 失败: %v\n", err)
		return
	}
	log.Printf("LoginV2 响应: success=%v, message=%s, token=%s\n",
		resp.Success, resp.Message, resp.Token)
	if resp.UserInfo != nil {
		log.Printf("用户信息: id=%s, username=%s, email=%s, phone=%s, nickname=%s\n",
			resp.UserInfo.UserId, resp.UserInfo.Username, resp.UserInfo.Email,
			resp.UserInfo.Phone, resp.UserInfo.Nickname)
	}
}

// TestUserUpdatesV1 测试V1版本用户状态更新接口
func TestUserUpdatesV1(ctx context.Context, client pb.UserClient) {
	log.Println("测试 GetUserUpdatesV1 接口...")
	stream, err := client.GetUserUpdatesV1(ctx, &pb.UserRequest{
		UserId: "testuser",
	})
	if err != nil {
		log.Printf("GetUserUpdatesV1 失败: %v\n", err)
		return
	}

	done := make(chan bool)
	go func() {
		for {
			update, err := stream.Recv()
			if err == io.EOF {
				done <- true
				return
			}
			if err != nil {
				log.Printf("接收更新失败: %v\n", err)
				done <- true
				return
			}
			log.Printf("收到V1状态更新: userId=%s, status=%s, timestamp=%v\n",
				update.UserId, update.Status, time.Unix(update.Timestamp, 0))
		}
	}()

	// 设置5秒超时
	select {
	case <-done:
		return
	case <-time.After(5 * time.Second):
		log.Printf("GetUserUpdatesV1 测试超时")
		return
	}
}

// TestUserUpdatesV2 测试V2版本用户状态更新接口
func TestUserUpdatesV2(ctx context.Context, client pb.UserClient) {
	log.Println("测试 GetUserUpdatesV2 接口...")
	stream, err := client.GetUserUpdatesV2(ctx, &pb.UserRequest{
		UserId: "testuser2",
	})
	if err != nil {
		log.Printf("GetUserUpdatesV2 失败: %v\n", err)
		return
	}

	done := make(chan bool)
	go func() {
		for {
			update, err := stream.Recv()
			if err == io.EOF {
				done <- true
				return
			}
			if err != nil {
				log.Printf("接收更新失败: %v\n", err)
				done <- true
				return
			}
			log.Printf("收到V2状态更新:\n"+
				"用户ID: %s\n"+
				"状态: %s\n"+
				"时间: %v\n"+
				"活动会话: %v\n"+
				"自定义状态: %v\n"+
				"位置: %s (%.4f, %.4f)\n"+
				"设备: %s (%s %s)\n",
				update.UserId,
				update.Status,
				time.Unix(update.Timestamp, 0),
				update.ActiveSessions,
				update.CustomStatus,
				update.LastLocation.Address,
				update.LastLocation.Latitude,
				update.LastLocation.Longitude,
				update.DeviceInfo.DeviceId,
				update.DeviceInfo.DeviceType,
				update.DeviceInfo.OsVersion)
		}
	}()

	// 设置5秒超时
	select {
	case <-done:
		return
	case <-time.After(5 * time.Second):
		log.Printf("GetUserUpdatesV2 测试超时")
		return
	}
}
