package services

import (
	"context"
	pb "demoapp/data"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/golang-jwt/jwt"
	"golang.org/x/crypto/bcrypt"
	"google.golang.org/grpc/peer"
)

var (
	// 用于JWT签名的密钥
	jwtSecret = []byte("your-secret-key")

	// 模拟数据库
	users   = make(map[string]*pb.UserInfo)
	usersMu sync.RWMutex

	// 存储用户密码的单独映射
	passwords = make(map[string]string) // key: userId, value: hashedPassword

	// 用户状态更新
	userUpdates   = make(map[string]chan *pb.UserUpdateV2)
	userUpdatesMu sync.RWMutex
)

// 获取客户端IP的辅助函数
func getClientIP(ctx context.Context) string {
	if p, ok := peer.FromContext(ctx); ok {
		return p.Addr.String()
	}
	return "unknown"
}

type UserServer struct {
	pb.UnimplementedUserServer
}

// NewUserServer 创建一个新的UserServer实例
func NewUserServer() *UserServer {
	return &UserServer{}
}

// 生成JWT token
func generateToken(userID string) (string, error) {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": userID,
		"exp":     time.Now().Add(time.Hour * 24).Unix(),
	})
	return token.SignedString(jwtSecret)
}

// RegisterV1 实现v1版本的用户注册
func (s *UserServer) RegisterV1(ctx context.Context, req *pb.RegisterRequest) (*pb.RegisterResponse, error) {
	startTime := time.Now()
	clientIP := getClientIP(ctx)
	log.Printf("[RegisterV1] 收到注册请求 - ClientIP: %s, Username: %s", clientIP, req.Username)

	// 检查用户名是否已存在
	usersMu.RLock()
	for _, u := range users {
		if u.Username == req.Username {
			usersMu.RUnlock()
			log.Printf("[RegisterV1] 注册失败 - ClientIP: %s, Username: %s, Reason: 用户名已存在, 处理时长: %v",
				clientIP, req.Username, time.Since(startTime))
			return nil, fmt.Errorf("用户名已存在")
		}
	}
	usersMu.RUnlock()

	// 加密密码
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		log.Printf("[RegisterV1] 注册失败 - ClientIP: %s, Username: %s, Error: %v, 处理时长: %v",
			clientIP, req.Username, err, time.Since(startTime))
		return nil, fmt.Errorf("密码加密失败: %v", err)
	}

	// 创建用户
	now := time.Now().Unix()
	userID := fmt.Sprintf("user_%d", len(users)+1)
	newUser := &pb.UserInfo{
		UserId:    userID,
		Username:  req.Username,
		Email:     req.Email,
		CreatedAt: now,
		LastLogin: now,
		Metadata:  make(map[string]string),
	}

	// 保存用户信息和密码
	usersMu.Lock()
	users[userID] = newUser
	passwords[userID] = string(hashedPassword)
	usersMu.Unlock()

	log.Printf("[RegisterV1] 注册成功 - ClientIP: %s, Username: %s, UserID: %s, 处理时长: %v",
		clientIP, req.Username, userID, time.Since(startTime))

	return &pb.RegisterResponse{
		Success:   true,
		UserId:    userID,
		Message:   "注册成功",
		CreatedAt: now,
	}, nil
}

// RegisterV2 实现v2版本的用户注册
func (s *UserServer) RegisterV2(ctx context.Context, req *pb.RegisterRequestV2) (*pb.RegisterResponse, error) {
	startTime := time.Now()
	clientIP := getClientIP(ctx)

	// 将请求转为JSON以便记录
	reqJSON, _ := json.Marshal(req)
	log.Printf("[RegisterV2] 收到注册请求 - ClientIP: %s, Request: %s", clientIP, string(reqJSON))

	// 检查用户名是否已存在
	usersMu.RLock()
	for _, u := range users {
		if u.Username == req.Username {
			usersMu.RUnlock()
			log.Printf("[RegisterV2] 注册失败 - ClientIP: %s, Username: %s, Reason: 用户名已存在, 处理时长: %v",
				clientIP, req.Username, time.Since(startTime))
			return nil, fmt.Errorf("用户名已存在")
		}
	}
	usersMu.RUnlock()

	// 加密密码
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		log.Printf("[RegisterV2] 注册失败 - ClientIP: %s, Username: %s, Error: %v, 处理时长: %v",
			clientIP, req.Username, err, time.Since(startTime))
		return nil, fmt.Errorf("密码加密失败: %v", err)
	}

	// 创建用户
	now := time.Now().Unix()
	userID := fmt.Sprintf("user_%d", len(users)+1)
	newUser := &pb.UserInfo{
		UserId:    userID,
		Username:  req.Username,
		Email:     req.Email,
		Phone:     req.Phone,
		Nickname:  req.Nickname,
		CreatedAt: now,
		LastLogin: now,
		Metadata:  req.Metadata,
	}

	// 保存用户信息和密码
	usersMu.Lock()
	users[userID] = newUser
	passwords[userID] = string(hashedPassword)
	usersMu.Unlock()

	log.Printf("[RegisterV2] 注册成功 - ClientIP: %s, Username: %s, UserID: %s, 处理时长: %v",
		clientIP, req.Username, userID, time.Since(startTime))

	return &pb.RegisterResponse{
		Success:   true,
		UserId:    userID,
		Message:   "注册成功",
		CreatedAt: now,
	}, nil
}

// LoginV1 实现v1版本的用户登录
func (s *UserServer) LoginV1(ctx context.Context, req *pb.LoginRequest) (*pb.LoginResponse, error) {
	startTime := time.Now()
	clientIP := getClientIP(ctx)
	log.Printf("[LoginV1] 收到登录请求 - ClientIP: %s, Username: %s", clientIP, req.Username)

	// 查找用户
	usersMu.RLock()
	var targetUser *pb.UserInfo
	var targetUserID string
	for id, u := range users {
		if u.Username == req.Username {
			targetUser = u
			targetUserID = id
			break
		}
	}
	usersMu.RUnlock()

	if targetUser == nil {
		log.Printf("[LoginV1] 登录失败 - ClientIP: %s, Username: %s, Reason: 用户不存在, 处理时长: %v",
			clientIP, req.Username, time.Since(startTime))
		return nil, fmt.Errorf("用户名或密码错误")
	}

	// 验证密码
	hashedPassword := passwords[targetUserID]
	if err := bcrypt.CompareHashAndPassword([]byte(hashedPassword), []byte(req.Password)); err != nil {
		log.Printf("[LoginV1] 登录失败 - ClientIP: %s, Username: %s, Reason: 密码错误, 处理时长: %v",
			clientIP, req.Username, time.Since(startTime))
		return nil, fmt.Errorf("用户名或密码错误")
	}

	// 更新最后登录时间
	now := time.Now().Unix()
	usersMu.Lock()
	targetUser.LastLogin = now
	usersMu.Unlock()

	log.Printf("[LoginV1] 登录成功 - ClientIP: %s, Username: %s, UserID: %s, 处理时长: %v",
		clientIP, req.Username, targetUser.UserId, time.Since(startTime))

	return &pb.LoginResponse{
		Success:  true,
		Token:    "dummy_token", // TODO: 实现真实的token生成
		Message:  "登录成功",
		UserInfo: targetUser,
	}, nil
}

// LoginV2 实现v2版本的用户登录
func (s *UserServer) LoginV2(ctx context.Context, req *pb.LoginRequestV2) (*pb.LoginResponse, error) {
	startTime := time.Now()
	clientIP := getClientIP(ctx)
	log.Printf("[LoginV2] 收到登录请求 - ClientIP: %s, LoginType: %s, Identifier: %s",
		clientIP, req.LoginType.String(), req.Identifier)

	usersMu.RLock()
	defer usersMu.RUnlock()

	// 根据不同的登录类型查找用户
	var user *pb.UserInfo
	for _, u := range users {
		switch req.LoginType {
		case pb.LoginRequestV2_USERNAME:
			if u.Username == req.Identifier {
				user = u
			}
		case pb.LoginRequestV2_EMAIL:
			if u.Email == req.Identifier {
				user = u
			}
		case pb.LoginRequestV2_PHONE:
			if u.Phone == req.Identifier {
				user = u
			}
		case pb.LoginRequestV2_TOKEN:
			// 验证token
			token, err := jwt.Parse(req.Credential, func(token *jwt.Token) (interface{}, error) {
				return jwtSecret, nil
			})
			if err == nil && token.Valid {
				if claims, ok := token.Claims.(jwt.MapClaims); ok {
					if userID, ok := claims["user_id"].(string); ok {
						user = users[userID]
					}
				}
			}
		}
		if user != nil {
			break
		}
	}

	if user == nil {
		log.Printf("[LoginV2] 登录失败 - ClientIP: %s, LoginType: %s, Identifier: %s, Reason: 用户不存在, 处理时长: %v",
			clientIP, req.LoginType.String(), req.Identifier, time.Since(startTime))
		return &pb.LoginResponse{
			Success: false,
			Message: "用户不存在或凭证无效",
		}, nil
	}

	// 生成新token
	token, err := generateToken(user.UserId)
	if err != nil {
		log.Printf("[LoginV2] 登录失败 - ClientIP: %s, LoginType: %s, Identifier: %s, Error: %v, 处理时长: %v",
			clientIP, req.LoginType.String(), req.Identifier, err, time.Since(startTime))
		return nil, fmt.Errorf("生成token失败: %v", err)
	}

	response := &pb.LoginResponse{
		Success:  true,
		Token:    token,
		Message:  "登录成功",
		UserInfo: user,
	}

	log.Printf("[LoginV2] 登录成功 - ClientIP: %s, LoginType: %s, Identifier: %s, UserID: %s, 处理时长: %v",
		clientIP, req.LoginType.String(), req.Identifier, user.UserId, time.Since(startTime))

	return response, nil
}

// GetUserUpdatesV1 实现v1版本的用户状态更新流
func (s *UserServer) GetUserUpdatesV1(req *pb.UserRequest, stream pb.User_GetUserUpdatesV1Server) error {
	startTime := time.Now()
	clientIP := getClientIP(stream.Context())
	log.Printf("[GetUserUpdatesV1] 开始订阅用户状态更新 - ClientIP: %s, UserID: %s",
		clientIP, req.UserId)

	// 检查用户是否存在
	usersMu.RLock()
	if _, exists := users[req.UserId]; !exists {
		usersMu.RUnlock()
		log.Printf("[GetUserUpdatesV1] 订阅失败 - ClientIP: %s, UserID: %s, Reason: 用户不存在",
			clientIP, req.UserId)
		return fmt.Errorf("用户不存在")
	}
	usersMu.RUnlock()

	// 创建更新通道
	updateChan := make(chan *pb.UserUpdate, 10)
	userUpdatesMu.Lock()
	userUpdates[req.UserId] = make(chan *pb.UserUpdateV2, 10)
	userUpdatesMu.Unlock()

	defer func() {
		userUpdatesMu.Lock()
		delete(userUpdates, req.UserId)
		userUpdatesMu.Unlock()
		close(updateChan)
		log.Printf("[GetUserUpdatesV1] 结束订阅 - ClientIP: %s, UserID: %s, 订阅时长: %v",
			clientIP, req.UserId, time.Since(startTime))
	}()

	// 发送初始状态
	initialUpdate := &pb.UserUpdate{
		UserId:    req.UserId,
		Status:    "online",
		Timestamp: time.Now().Unix(),
	}
	if err := stream.Send(initialUpdate); err != nil {
		log.Printf("[GetUserUpdatesV1] 发送更新失败 - ClientIP: %s, UserID: %s, Error: %v",
			clientIP, req.UserId, err)
		return err
	}

	log.Printf("[GetUserUpdatesV1] 发送初始状态 - ClientIP: %s, UserID: %s, Status: %s",
		clientIP, req.UserId, initialUpdate.Status)

	// 持续发送更新
	for {
		select {
		case <-stream.Context().Done():
			log.Printf("[GetUserUpdatesV1] 客户端断开连接 - ClientIP: %s, UserID: %s, 订阅时长: %v",
				clientIP, req.UserId, time.Since(startTime))
			return nil
		case update := <-updateChan:
			if err := stream.Send(update); err != nil {
				log.Printf("[GetUserUpdatesV1] 发送更新失败 - ClientIP: %s, UserID: %s, Error: %v",
					clientIP, req.UserId, err)
				return err
			}
			log.Printf("[GetUserUpdatesV1] 发送状态更新 - ClientIP: %s, UserID: %s, Status: %s",
				clientIP, req.UserId, update.Status)
		}
	}
}

// GetUserUpdatesV2 实现v2版本的用户状态更新流
func (s *UserServer) GetUserUpdatesV2(req *pb.UserRequest, stream pb.User_GetUserUpdatesV2Server) error {
	startTime := time.Now()
	clientIP := getClientIP(stream.Context())
	log.Printf("[GetUserUpdatesV2] 开始订阅用户状态更新 - ClientIP: %s, UserID: %s",
		clientIP, req.UserId)

	// 检查用户是否存在
	usersMu.RLock()
	user, exists := users[req.UserId]
	if !exists {
		usersMu.RUnlock()
		log.Printf("[GetUserUpdatesV2] 订阅失败 - ClientIP: %s, UserID: %s, Reason: 用户不存在",
			clientIP, req.UserId)
		return fmt.Errorf("用户不存在")
	}
	usersMu.RUnlock()

	log.Printf("[GetUserUpdatesV2] 用户 %s (%s) 开始订阅更新", user.Username, user.UserId)

	// 创建更新通道
	updateChan := make(chan *pb.UserUpdateV2, 10)
	userUpdatesMu.Lock()
	userUpdates[req.UserId] = make(chan *pb.UserUpdateV2, 10)
	userUpdatesMu.Unlock()

	defer func() {
		userUpdatesMu.Lock()
		delete(userUpdates, req.UserId)
		userUpdatesMu.Unlock()
		close(updateChan)
		log.Printf("[GetUserUpdatesV2] 结束订阅 - ClientIP: %s, UserID: %s, 订阅时长: %v",
			clientIP, req.UserId, time.Since(startTime))
	}()

	// 发送初始状态
	initialUpdate := &pb.UserUpdateV2{
		UserId:         req.UserId,
		Status:         "online",
		Timestamp:      time.Now().Unix(),
		ActiveSessions: []string{clientIP},
		CustomStatus:   map[string]string{"mood": "happy"},
		LastLocation: &pb.Location{
			Latitude:  0,
			Longitude: 0,
			Address:   "未知",
			UpdatedAt: time.Now().Unix(),
		},
		DeviceInfo: &pb.DeviceInfo{
			DeviceId:   fmt.Sprintf("device_%s", clientIP),
			DeviceType: "unknown",
			OsVersion:  "unknown",
			AppVersion: "1.0.0",
			DeviceMetadata: map[string]string{
				"ip": clientIP,
			},
		},
	}

	if err := stream.Send(initialUpdate); err != nil {
		log.Printf("[GetUserUpdatesV2] 发送更新失败 - ClientIP: %s, UserID: %s, Error: %v",
			clientIP, req.UserId, err)
		return err
	}

	updateJSON, _ := json.Marshal(initialUpdate)
	log.Printf("[GetUserUpdatesV2] 发送初始状态 - ClientIP: %s, UserID: %s, Update: %s",
		clientIP, req.UserId, string(updateJSON))

	// 持续发送更新
	for {
		select {
		case <-stream.Context().Done():
			log.Printf("[GetUserUpdatesV2] 客户端断开连接 - ClientIP: %s, UserID: %s, 订阅时长: %v",
				clientIP, req.UserId, time.Since(startTime))
			return nil
		case update := <-updateChan:
			if err := stream.Send(update); err != nil {
				log.Printf("[GetUserUpdatesV2] 发送更新失败 - ClientIP: %s, UserID: %s, Error: %v",
					clientIP, req.UserId, err)
				return err
			}
			updateJSON, _ := json.Marshal(update)
			log.Printf("[GetUserUpdatesV2] 发送状态更新 - ClientIP: %s, UserID: %s, Update: %s",
				clientIP, req.UserId, string(updateJSON))
		}
	}
}
