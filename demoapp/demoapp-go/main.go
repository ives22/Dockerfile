package main

import (
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
)

var (
	// 应用版本，从环境变量读取，默认为 "v2.0"
	appVersion = getEnv("VERSION", "v2.0")
	
	// 健康检查状态，默认为 "OK"
	livezStatus = "OK"
	readyzStatus = "OK"
	
	// 从环境变量获取服务名称
	currentServiceName = getEnv("APP_NAME", "demoapp")
	
	// 随机选择一个颜色
	randomColor = generateRandomColor()
)

// 生成随机颜色
func generateRandomColor() string {
	rand.Seed(time.Now().UnixNano())
	return fmt.Sprintf("#%06x", rand.Intn(0xFFFFFF))
}

// 获取环境变量，如果不存在则返回默认值
func getEnv(key, defaultValue string) string {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}
	return value
}

// 记录请求信息的中间件
func loggerMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// 处理请求前
		startTime := time.Now()
		
		// 处理请求
		c.Next()
		
		// 处理请求后
		latency := time.Since(startTime)
		clientIP := c.ClientIP()
		method := c.Request.Method
		statusCode := c.Writer.Status()
		path := c.Request.URL.Path
		query := c.Request.URL.RawQuery
		
		if query != "" {
			path = path + "?" + query
		}
		
		// 使用传统的Go日志格式
		log.Printf("[GIN] %v | %3d | %13v | %15s | %-7s %s",
			time.Now().Format("2006/01/02 - 15:04:05"),
			statusCode,
			latency,
			clientIP,
			method,
			path,
		)
	}
}

func main() {
	// 设置日志格式
	log.SetFlags(log.Ldate | log.Ltime)
	log.SetOutput(os.Stdout)
	
	// 创建 Gin 路由
	router := gin.New()
	
	// 使用自定义日志中间件
	router.Use(loggerMiddleware())
	
	// 设置路由
	setupRoutes(router)
	
	// 获取主机和端口
	host := getEnv("HOST", "0.0.0.0")
	portStr := getEnv("PORT", "80")
	port, err := strconv.Atoi(portStr)
	if err != nil {
		port = 80
	}
	
	// 解析命令行参数
	args := os.Args[1:]
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "-p", "--port":
			if i+1 < len(args) {
				portVal, err := strconv.Atoi(args[i+1])
				if err == nil {
					port = portVal
				}
				i++
			}
		case "-l", "--host":
			if i+1 < len(args) {
				host = args[i+1]
				i++
			}
		case "-v", "--verbose":
			gin.SetMode(gin.DebugMode)
		}
	}
	
	// 默认使用生产模式
	if gin.Mode() != gin.DebugMode {
		gin.SetMode(gin.ReleaseMode)
	}
	
	// 启动服务器
	addr := fmt.Sprintf("%s:%d", host, port)
	log.Printf("服务启动在 %s\n", addr)
	if err := router.Run(addr); err != nil {
		log.Fatalf("启动服务失败: %v", err)
	}
}

// 设置所有路由
func setupRoutes(router *gin.Engine) {
	// 欢迎页面
	router.GET("/", welcomeHandler)
	
	// 健康检查端点
	router.GET("/livez", livezHandler)
	router.POST("/livez", livezHandler)
	router.HEAD("/livez", livezHandler)
	
	router.GET("/readyz", readyzHandler)
	router.POST("/readyz", readyzHandler)
	router.HEAD("/readyz", readyzHandler)
	
	// API 端点
	router.GET("/api/get_service", getServiceNameHandler)
	router.GET("/hostname", hostnameHandler)
	router.GET("/configs", configsHandler)
	router.GET("/user-agent", userAgentHandler)
	
	// 添加延迟接口
	router.GET("/delay", delayHandler)
}

// 欢迎页面处理函数
func welcomeHandler(c *gin.Context) {
	serverName, _ := os.Hostname()
	clientIP := c.ClientIP()
	
	// 获取服务器 IP（简化版本）
	serverIP := "127.0.0.1" // 简化处理，实际应该获取真实 IP
	
	userAgent := c.GetHeader("User-Agent")
	
	// 检查是否是命令行工具
	if strings.Contains(strings.ToLower(userAgent), "curl") || 
	   strings.Contains(strings.ToLower(userAgent), "wget") || 
	   strings.Contains(strings.ToLower(userAgent), "elinks") {
		// 返回纯文本
		welcomeMessage := fmt.Sprintf("Demoapp by vvoo! App Version: %s, Client IP: %s, Server Name: %s, Server IP: %s ~\n", 
			appVersion, clientIP, serverName, serverIP)
		c.String(http.StatusOK, welcomeMessage)
	} else {
		// 返回 HTML
		serverNameHTML := fmt.Sprintf("<span style=\"color: %s;\">%s</span>", randomColor, serverName)
		serverVersionHTML := fmt.Sprintf("<span style=\"color: red;\">%s</span>", appVersion)
		welcomeMessage := fmt.Sprintf(`
		<html>
		<head><title>Welcome</title></head>
		<body>
			<p>Demoapp by vvoo! App Version: %s, Client IP: %s, Server Name: %s, Server IP: %s ~</p>
		</body>
		</html>
		`, serverVersionHTML, clientIP, serverNameHTML, serverIP)
		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, welcomeMessage)
	}
}

// 处理健康检查
func handleHealthCheck(c *gin.Context, statusType string) {
	var statusValue string
	
	switch c.Request.Method {
	case "POST":
		// 更新状态
		statusValue = c.PostForm(statusType)
		if statusValue == "" {
			statusValue = "OK"
		}
		
		if statusType == "livez" {
			livezStatus = statusValue
		} else {
			readyzStatus = statusValue
		}
		
		c.JSON(http.StatusOK, gin.H{statusType: statusValue})
		
	case "GET":
		// 获取状态
		if statusType == "livez" {
			statusValue = livezStatus
		} else {
			statusValue = readyzStatus
		}
		
		if statusValue == "OK" {
			c.String(http.StatusOK, statusValue)
		} else {
			c.String(506, statusValue) // 使用 506 状态码表示不健康
		}
		
	case "HEAD":
		// HEAD 请求只返回状态码
		if statusType == "livez" {
			statusValue = livezStatus
		} else {
			statusValue = readyzStatus
		}
		
		if statusValue == "OK" {
			c.Status(http.StatusOK)
		} else {
			c.Status(506) // 使用 506 状态码表示不健康
		}
	}
}

// 存活探针处理函数
func livezHandler(c *gin.Context) {
	handleHealthCheck(c, "livez")
}

// 就绪探针处理函数
func readyzHandler(c *gin.Context) {
	handleHealthCheck(c, "readyz")
}

// 获取服务名称处理函数
func getServiceNameHandler(c *gin.Context) {
	serverName, _ := os.Hostname()
	c.JSON(http.StatusOK, gin.H{
		"service_name":  currentServiceName,
		"instance_name": serverName,
		"app_version":   appVersion,
	})
}

// 主机名处理函数
func hostnameHandler(c *gin.Context) {
	hostname, _ := os.Hostname()
	c.String(http.StatusOK, "ServerName: %s\n", hostname)
}

// 配置信息处理函数
func configsHandler(c *gin.Context) {
	deployEnv := os.Getenv("DEPLOYENV")
	release := os.Getenv("RELEASE")
	c.String(http.StatusOK, "DEPLOYENV: %s\nRELEASE: %s\n", deployEnv, release)
}

// 用户代理处理函数
func userAgentHandler(c *gin.Context) {
	userAgent := c.GetHeader("User-Agent")
	c.String(http.StatusOK, "User-Agent: %s\n", userAgent)
}

// 延迟处理函数 - 随机延迟0-3000毫秒
func delayHandler(c *gin.Context) {
	// 初始化随机数生成器
	rand.Seed(time.Now().UnixNano())
	
	// 生成0-3000毫秒的随机延迟
	delayMs := rand.Intn(3001) // 0 到 3000 之间的随机数
	
	// 记录开始时间
	startTime := time.Now()
	
	// 休眠指定时间
	time.Sleep(time.Duration(delayMs) * time.Millisecond)
	
	// 计算实际延迟时间
	actualDelay := time.Since(startTime).Milliseconds()
	
	// 返回延迟信息
	c.JSON(http.StatusOK, gin.H{
		"requested_delay_ms": delayMs,
		"actual_delay_ms":    actualDelay,
		"timestamp":          time.Now().Format(time.RFC3339),
	})
} 