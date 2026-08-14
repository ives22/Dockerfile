package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
)

var (
	// 应用版本，从环境变量读取，默认为 "v2.0"
	appVersion = getEnv("VERSION", "v2.0")

	// 从环境变量获取服务名称
	currentServiceName = getEnv("APP_NAME", "demoapp")

	// 随机选择一个颜色
	randomColor = generateRandomColor()
)

const maxDelaySeconds = 300

type diagnosticConfig struct {
	HTTPPort       string
	Namespace      string
	Ingress        string
	Service        string
	Pod            string
	PodIP          string
	ServiceIP      string
	Version        string
	TrustedProxies []string
	StressToken    string
}

type requestInfoResponse struct {
	Path       string              `json:"path"`
	RequestURI string              `json:"requestURI"`
	Query      map[string][]string `json:"query"`
	Host       string              `json:"host"`
	Method     string              `json:"method"`
	Proto      string              `json:"proto"`
	Headers    http.Header         `json:"headers"`
	ClientIP   string              `json:"clientIP"`
	RemoteIP   string              `json:"remoteIP"`
	HTTPPort   string              `json:"httpPort"`
	Namespace  string              `json:"namespace"`
	Ingress    string              `json:"ingress"`
	Service    string              `json:"service"`
	Pod        string              `json:"pod"`
}

type ipInfoResponse struct {
	ClientIP  string `json:"clientIP"`
	RemoteIP  string `json:"remoteIP"`
	PodIP     string `json:"podIP"`
	ServiceIP string `json:"serviceIP"`
}

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

func loadDiagnosticConfig(port int) diagnosticConfig {
	config := diagnosticConfig{
		HTTPPort:    strconv.Itoa(port),
		Namespace:   os.Getenv("NAMESPACE"),
		Ingress:     os.Getenv("INGRESS_NAME"),
		Service:     os.Getenv("SERVICE_NAME"),
		Pod:         os.Getenv("POD_NAME"),
		PodIP:       os.Getenv("POD_IP"),
		ServiceIP:   os.Getenv("SERVICE_IP"),
		Version:     appVersion,
		StressToken: os.Getenv("STRESS_TOKEN"),
	}
	if trustedProxies := strings.TrimSpace(os.Getenv("TRUSTED_PROXIES")); trustedProxies != "" {
		for _, proxy := range strings.Split(trustedProxies, ",") {
			if proxy = strings.TrimSpace(proxy); proxy != "" {
				config.TrustedProxies = append(config.TrustedProxies, proxy)
			}
		}
	}
	return config
}

func remoteIP(remoteAddr string) string {
	host, _, err := net.SplitHostPort(remoteAddr)
	if err == nil {
		return host
	}

	return strings.Trim(remoteAddr, "[]")
}

func main() {
	// 设置日志格式
	log.SetFlags(log.Ldate | log.Ltime)
	log.SetOutput(os.Stdout)

	// 获取主机和端口
	host := getEnv("HOST", "0.0.0.0")
	portStr := getEnv("PORT", "80")
	port, err := strconv.Atoi(portStr)
	if err != nil {
		port = 80
	}

	// 解析命令行参数
	args := os.Args[1:]
	verbose := false
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
			verbose = true
		}
	}

	// 必须在创建路由前设置模式，否则 Gin 会输出 Debug 路由表。
	if verbose {
		gin.SetMode(gin.DebugMode)
	} else if _, configured := os.LookupEnv("GIN_MODE"); !configured {
		gin.SetMode(gin.ReleaseMode)
	}
	router := gin.New()

	config := loadDiagnosticConfig(port)
	state := newAppState(config)
	setupRoutesWithState(router, config, state)

	addr := fmt.Sprintf("%s:%d", host, port)
	server := &http.Server{
		Addr:              addr,
		Handler:           router,
		ReadHeaderTimeout: 10 * time.Second,
	}
	protocolServers, err := startOptionalProtocolServers(host, router, config)
	if err != nil {
		log.Fatalf("启动可选协议服务失败: %v", err)
	}

	shutdownSignal := make(chan os.Signal, 1)
	shutdownDone := make(chan struct{})
	signal.Notify(shutdownSignal, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		defer close(shutdownDone)
		<-shutdownSignal
		signal.Stop(shutdownSignal)
		state.beginShutdown()
		shutdownTimeout, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()
		protocolShutdownDone := make(chan struct{})
		go func() {
			protocolServers.Shutdown(shutdownTimeout)
			close(protocolShutdownDone)
		}()
		if err := server.Shutdown(shutdownTimeout); err != nil {
			log.Printf("HTTP 服务优雅关闭失败: %v", err)
		}
		<-protocolShutdownDone
	}()

	log.Printf("服务启动在 %s\n", addr)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatalf("启动服务失败: %v", err)
	}
	<-shutdownDone
}

// 设置所有路由
func setupRoutes(router *gin.Engine, config diagnosticConfig) {
	setupRoutesWithState(router, config, newAppState(config))
}

func setupRoutesWithState(router *gin.Engine, config diagnosticConfig, state *appState) {
	if err := router.SetTrustedProxies(nil); err != nil {
		log.Printf("禁用默认可信代理失败: %v", err)
	}
	if len(config.TrustedProxies) > 0 {
		if err := router.SetTrustedProxies(config.TrustedProxies); err != nil {
			_ = router.SetTrustedProxies(nil)
			log.Printf("忽略无效的 TRUSTED_PROXIES 配置并禁用转发头信任: %v", err)
		}
	}
	router.Use(diagnosticsMiddleware(state))

	// 未注册的路径或请求方法统一返回欢迎页内容
	router.HandleMethodNotAllowed = true
	router.NoRoute(welcomeHandler)
	router.NoMethod(welcomeHandler)

	// 欢迎页面
	router.GET("/", welcomeHandler)

	// 健康检查端点
	router.GET("/livez", healthHandler(state, "livez"))
	router.POST("/livez", healthHandler(state, "livez"))
	router.HEAD("/livez", healthHandler(state, "livez"))

	router.GET("/readyz", healthHandler(state, "readyz"))
	router.POST("/readyz", healthHandler(state, "readyz"))
	router.HEAD("/readyz", healthHandler(state, "readyz"))

	// API 端点
	router.GET("/api/get_service", getServiceNameHandler)
	router.GET("/hostname", hostnameHandler)
	router.GET("/configs", configsHandler)
	router.GET("/user-agent", userAgentHandler)

	// 请求和网络诊断端点
	requestMethods := []string{
		http.MethodGet,
		http.MethodPost,
		http.MethodPut,
		http.MethodPatch,
		http.MethodDelete,
		http.MethodOptions,
		http.MethodHead,
	}
	for _, method := range requestMethods {
		router.Handle(method, "/request", requestInfoHandler(config))
	}
	router.GET("/ip", ipInfoHandler(config))
	router.GET("/status/:code", statusHandler)
	router.GET("/flaky", flakyHandler(state, config))
	router.GET("/stats", statsHandler(state))
	registerHTTPToolRoutes(router, config, state)
	registerStressRoutes(router, config)

	// 添加延迟接口
	router.GET("/delay", delayHandler)
}

func requestInfoHandler(config diagnosticConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		response := newRequestInfoResponse(c.Request, config, c.ClientIP())
		if c.Request.Method == http.MethodHead {
			payload, err := json.MarshalIndent(response, "", "    ")
			if err != nil {
				c.JSON(http.StatusInternalServerError, apiError("RESPONSE_ENCODING_FAILED", "无法编码请求诊断响应"))
				return
			}
			c.Header("Content-Type", "application/json; charset=utf-8")
			c.Header("Content-Length", strconv.Itoa(len(payload)))
			c.Status(http.StatusOK)
			return
		}

		c.IndentedJSON(http.StatusOK, response)
	}
}

func newRequestInfoResponse(request *http.Request, config diagnosticConfig, clientIP string) requestInfoResponse {
	return requestInfoResponse{
		Path:       request.URL.Path,
		RequestURI: request.URL.RequestURI(),
		Query:      request.URL.Query(),
		Host:       request.Host,
		Method:     request.Method,
		Proto:      request.Proto,
		Headers:    request.Header,
		ClientIP:   clientIP,
		RemoteIP:   remoteIP(request.RemoteAddr),
		HTTPPort:   config.HTTPPort,
		Namespace:  config.Namespace,
		Ingress:    config.Ingress,
		Service:    config.Service,
		Pod:        config.Pod,
	}
}

func ipInfoHandler(config diagnosticConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.IndentedJSON(http.StatusOK, ipInfoResponse{
			ClientIP:  c.ClientIP(),
			RemoteIP:  remoteIP(c.Request.RemoteAddr),
			PodIP:     config.PodIP,
			ServiceIP: config.ServiceIP,
		})
	}
}

func statusHandler(c *gin.Context) {
	statusCode, err := strconv.Atoi(c.Param("code"))
	if err != nil || statusCode < 200 || statusCode > 599 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error": gin.H{
				"code":    "INVALID_STATUS_CODE",
				"message": "状态码必须是 200 到 599 之间的整数",
			},
		})
		return
	}

	c.JSON(statusCode, gin.H{
		"status":    statusCode,
		"requestID": c.GetString("requestID"),
	})
}

func flakyHandler(state *appState, config diagnosticConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		key := strings.TrimSpace(c.Query("key"))
		failures, failuresErr := strconv.Atoi(c.DefaultQuery("failures", "1"))
		statusCode, statusErr := strconv.Atoi(c.DefaultQuery("status", "503"))
		if key == "" || len(key) > 128 || failuresErr != nil || failures < 0 || failures > 100 || statusErr != nil || statusCode < 400 || statusCode > 599 {
			c.JSON(http.StatusBadRequest, gin.H{
				"error": gin.H{
					"code":    "INVALID_FLAKY_CONFIG",
					"message": "key 必填且不超过 128 字符，failures 必须为 0 到 100，status 必须为 400 到 599",
				},
			})
			return
		}

		attempt, accepted := state.nextFlakyAttempt(key)
		if !accepted {
			c.JSON(http.StatusTooManyRequests, apiError("FLAKY_KEY_LIMIT_REACHED", "当前 Pod 的 flaky key 数量已达到上限"))
			return
		}
		failed := attempt <= failures
		responseStatus := http.StatusOK
		if failed {
			responseStatus = statusCode
		}
		c.JSON(responseStatus, gin.H{
			"key":       key,
			"attempt":   attempt,
			"failures":  failures,
			"failed":    failed,
			"status":    responseStatus,
			"pod":       config.Pod,
			"requestID": c.GetString("requestID"),
		})
	}
}

func statsHandler(state *appState) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.IndentedJSON(http.StatusOK, state.snapshot())
	}
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
func healthHandler(state *appState, statusType string) gin.HandlerFunc {
	return func(c *gin.Context) {
		if c.Request.Method == http.MethodPost {
			statusValue := c.PostForm(statusType)
			if statusValue == "" {
				statusValue = "OK"
			}
			state.setHealth(statusType, statusValue)
			c.JSON(http.StatusOK, gin.H{statusType: statusValue})
			return
		}

		statusValue := state.health(statusType)
		if statusValue == "OK" {
			if c.Request.Method == http.MethodHead {
				c.Status(http.StatusOK)
				return
			}
			c.String(http.StatusOK, statusValue)
			return
		}
		if c.Request.Method == http.MethodHead {
			c.Status(506)
			return
		}
		c.String(506, statusValue)
	}
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

func parseDelaySeconds(value string) (time.Duration, error) {
	seconds, err := strconv.Atoi(value)
	if err != nil || seconds < 0 || seconds > maxDelaySeconds {
		return 0, fmt.Errorf("seconds 必须是 0 到 %d 之间的整数", maxDelaySeconds)
	}

	return time.Duration(seconds) * time.Second, nil
}

// 延迟处理函数 - 默认随机延迟0-3000毫秒，可通过 seconds 指定固定秒数
func delayHandler(c *gin.Context) {
	delay := time.Duration(rand.Intn(3001)) * time.Millisecond
	if seconds, configured := c.GetQuery("seconds"); configured {
		var err error
		delay, err = parseDelaySeconds(seconds)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{
				"error": gin.H{
					"code":    "INVALID_DELAY_SECONDS",
					"message": err.Error(),
				},
			})
			return
		}
	}

	// 记录开始时间
	startTime := time.Now()

	// 等待指定时间；客户端取消请求时立即释放处理协程
	if !waitForRequest(c.Request.Context().Done(), delay) {
		return
	}

	// 计算实际延迟时间
	actualDelay := time.Since(startTime).Milliseconds()

	// 返回延迟信息
	c.JSON(http.StatusOK, gin.H{
		"requested_delay_ms": delay.Milliseconds(),
		"actual_delay_ms":    actualDelay,
		"timestamp":          time.Now().Format(time.RFC3339),
	})
}
