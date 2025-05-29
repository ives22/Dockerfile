package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"
)

var (
	// 后端服务URL，从环境变量读取，默认为 http://localhost:80
	backendURL = getEnv("BACKEND_URL", "http://localhost:80")
	
	// 监听地址，默认为 0.0.0.0
	listenAddr = getEnv("LISTEN", "0.0.0.0")
	
	// 监听端口，默认为 5901
	listenPort = getEnvInt("PORT", 5901)
	
	// 服务名称
	serviceName = "frontend"
	
	// 代理版本，从环境变量读取，默认为 v1.0
	proxyVersion = getEnv("PROXY_VERSION", "v1.0")
)

// 获取环境变量，如果不存在则返回默认值
func getEnv(key, defaultValue string) string {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}
	return value
}

// 获取整型环境变量，如果不存在或无法解析则返回默认值
func getEnvInt(key string, defaultValue int) int {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}
	
	intValue, err := strconv.Atoi(value)
	if err != nil {
		return defaultValue
	}
	
	return intValue
}

// 记录请求的中间件
func loggingMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		
		// 处理请求
		next.ServeHTTP(w, r)
		
		// 记录请求信息
		duration := time.Since(start)
		log.Printf("[Frontend] %s | %s | %s | %v",
			r.Method,
			r.RequestURI,
			r.RemoteAddr,
			duration,
		)
	})
}

// 服务信息处理函数
func servicesHandler(w http.ResponseWriter, r *http.Request) {
	// 记录开始时间
	startTime := time.Now()
	
	// 构建后端API URL
	apiURL := fmt.Sprintf("%s/api/get_service", backendURL)
	
	// 创建HTTP客户端
	client := &http.Client{
		Timeout: 5 * time.Second,
	}
	
	// 发送请求到后端
	resp, err := client.Get(apiURL)
	if err != nil {
		log.Printf("Error connecting to backend: %v", err)
		http.Error(w, "Error connecting to backend service", http.StatusServiceUnavailable)
		return
	}
	defer resp.Body.Close()
	
	// 读取响应内容
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("Error reading backend response: %v", err)
		http.Error(w, "Error reading backend response", http.StatusInternalServerError)
		return
	}
	
	// 计算请求耗时
	elapsedTime := time.Since(startTime)
	
	// 解析后端返回的JSON
	var backendData interface{}
	if err := json.Unmarshal(body, &backendData); err != nil {
		log.Printf("Error parsing backend JSON: %v", err)
		http.Error(w, "Error parsing backend response", http.StatusInternalServerError)
		return
	}
	
	// 构建新的响应，包含标识和耗时
	response := map[string]interface{}{
		"frontend_proxy": proxyVersion,
		"elapsed_ms":     elapsedTime.Milliseconds(),
		"backend_data":   backendData,
	}
	
	// 转换为JSON
	jsonResponse, err := json.MarshalIndent(response, "", "  ")
	if err != nil {
		http.Error(w, "Error creating response", http.StatusInternalServerError)
		return
	}
	
	// 设置响应头
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("X-Proxy-Version", proxyVersion)
	w.WriteHeader(http.StatusOK)
	
	// 返回后端响应
	w.Write(jsonResponse)
}

// 获取前端服务名称的处理函数
func getServiceNameHandler(w http.ResponseWriter, r *http.Request) {
	// 获取主机名
	hostname, err := os.Hostname()
	if err != nil {
		hostname = "unknown"
	}
	
	// 构建响应
	response := map[string]string{
		"service_name":  serviceName,
		"instance_name": hostname,
		"app_version":   proxyVersion,
	}
	
	// 转换为JSON
	jsonResponse, err := json.Marshal(response)
	if err != nil {
		http.Error(w, "Error creating response", http.StatusInternalServerError)
		return
	}
	
	// 设置响应头并返回
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write(jsonResponse)
}

// 创建反向代理
func createReverseProxy() (*httputil.ReverseProxy, error) {
	// 解析后端URL
	backendURLParsed, err := url.Parse(backendURL)
	if err != nil {
		return nil, err
	}
	
	// 创建反向代理
	proxy := httputil.NewSingleHostReverseProxy(backendURLParsed)
	
	// 自定义Director函数，用于修改请求
	originalDirector := proxy.Director
	proxy.Director = func(req *http.Request) {
		originalDirector(req)
		
		// 只传递特定的请求头
		if req.URL.Path == "/" {
			// 如果存在 x-canary 头，则传递
			if canaryHeader := req.Header.Get("x-canary"); canaryHeader != "" {
				req.Header.Set("x-canary", canaryHeader)
			} else {
				req.Header.Del("x-canary")
			}
			
			// 如果存在 x-user 头，则传递
			if userHeader := req.Header.Get("x-user"); userHeader != "" {
				req.Header.Set("x-user", userHeader)
			} else {
				req.Header.Del("x-user")
			}
		} else {
			// 对于其他路径，不传递这些头
			req.Header.Del("x-canary")
			req.Header.Del("x-user")
		}
		
		// 添加代理信息
		req.Header.Set("X-Forwarded-By", "frontend")
		req.Header.Set("X-Proxy-Version", proxyVersion)
	}
	
	// 自定义响应修改
	proxy.ModifyResponse = func(resp *http.Response) error {
		// 获取原始响应内容
		originalBody, err := io.ReadAll(resp.Body)
		if err != nil {
			return err
		}
		resp.Body.Close()
		
		// 计算请求耗时（从请求上下文中获取开始时间）
		var elapsedTime time.Duration
		if startTime, ok := resp.Request.Context().Value("startTime").(time.Time); ok {
			elapsedTime = time.Since(startTime)
		}
		
		// 构建新的响应内容
		var newBody string
		contentType := resp.Header.Get("Content-Type")
		
		// 根据内容类型处理不同的响应
		if strings.Contains(contentType, "application/json") {
			// JSON响应添加包装
			newBody = fmt.Sprintf(`{
  "frontend_proxy": "%s",
  "elapsed_ms": %d,
  "backend_data": %s
}`, proxyVersion, elapsedTime.Milliseconds(), originalBody)
		} else if strings.Contains(contentType, "text/html") {
			// HTML响应添加标识
			htmlStr := string(originalBody)
			
			// 检查是否有 <body> 标签
			if bodyIndex := strings.Index(strings.ToLower(htmlStr), "<body>"); bodyIndex != -1 {
				// 在 <body> 标签后插入标识
				newBody = htmlStr[:bodyIndex+6] + 
					fmt.Sprintf("\n<!-- Frontend Proxy %s, Request Time: %dms -->\n", proxyVersion, elapsedTime.Milliseconds()) + 
					htmlStr[bodyIndex+6:]
			} else if headIndex := strings.Index(strings.ToLower(htmlStr), "</head>"); headIndex != -1 {
				// 如果没有 <body> 但有 </head>，在 </head> 后插入
				newBody = htmlStr[:headIndex+7] + 
					fmt.Sprintf("\n<!-- Frontend Proxy %s, Request Time: %dms -->\n", proxyVersion, elapsedTime.Milliseconds()) + 
					htmlStr[headIndex+7:]
			} else {
				// 如果都没有，在开头插入
				newBody = fmt.Sprintf("<!-- Frontend Proxy %s, Request Time: %dms -->\n", proxyVersion, elapsedTime.Milliseconds()) + 
					htmlStr
			}
			
			// 在页面顶部添加可见的标识栏
			bannerStyle := `
<div style="background-color: #f8f9fa; color: #212529; padding: 10px; text-align: center; font-family: Arial, sans-serif; border-bottom: 1px solid #dee2e6; position: sticky; top: 0; z-index: 1000;">
  Frontend Proxy %s | Request Time: %dms
</div>
`
			// 在 <body> 标签后插入标识栏
			if bodyIndex := strings.Index(strings.ToLower(newBody), "<body>"); bodyIndex != -1 {
				newBody = newBody[:bodyIndex+6] + 
					fmt.Sprintf(bannerStyle, proxyVersion, elapsedTime.Milliseconds()) + 
					newBody[bodyIndex+6:]
			}
		} else {
			// 纯文本响应添加前缀
			newBody = fmt.Sprintf("Frontend Proxy %s | Request Time: %dms\n%s", 
				proxyVersion, elapsedTime.Milliseconds(), string(originalBody))
		}
		
		// 设置新的响应体
		resp.Body = io.NopCloser(strings.NewReader(newBody))
		resp.ContentLength = int64(len(newBody))
		resp.Header.Set("Content-Length", strconv.Itoa(len(newBody)))
		resp.Header.Set("X-Proxy-Version", proxyVersion)
		
		return nil
	}
	
	// 自定义错误处理
	proxy.ErrorHandler = func(w http.ResponseWriter, r *http.Request, err error) {
		log.Printf("Proxy error: %v", err)
		http.Error(w, "Backend service unavailable", http.StatusServiceUnavailable)
	}
	
	return proxy, nil
}

// 注册代理处理函数，处理所有其他请求
func proxyHandler(proxy *httputil.ReverseProxy) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// 如果请求路径不是 / 开头，则添加 /
		if !strings.HasPrefix(r.URL.Path, "/") {
			r.URL.Path = "/" + r.URL.Path
		}
		
		// 在上下文中保存开始时间，用于计算耗时
		startTime := time.Now()
		ctx := context.WithValue(r.Context(), "startTime", startTime)
		r = r.WithContext(ctx)
		
		// 使用反向代理处理请求
		proxy.ServeHTTP(w, r)
	}
}

func main() {
	// 设置日志格式
	log.SetFlags(log.Ldate | log.Ltime)
	log.SetOutput(os.Stdout)
	
	// 创建反向代理
	proxy, err := createReverseProxy()
	if err != nil {
		log.Fatalf("Error creating reverse proxy: %v", err)
	}
	
	// 创建路由
	mux := http.NewServeMux()
	
	// 注册服务信息处理函数
	mux.HandleFunc("/services", servicesHandler)
	
	// 注册获取服务名称处理函数
	mux.HandleFunc("/api/get_service", getServiceNameHandler)
	
	// 注册代理处理函数，处理所有其他请求
	mux.HandleFunc("/", proxyHandler(proxy))
	
	// 创建带日志中间件的处理器
	handler := loggingMiddleware(mux)
	
	// 构建监听地址
	addr := fmt.Sprintf("%s:%d", listenAddr, listenPort)
	
	// 启动服务器
	log.Printf("Frontend service starting on %s, connecting to backend at %s", addr, backendURL)
	if err := http.ListenAndServe(addr, handler); err != nil {
		log.Fatalf("Error starting server: %v", err)
	}
} 