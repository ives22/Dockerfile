package main

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
)

func newTestRouter(config diagnosticConfig) *gin.Engine {
	gin.SetMode(gin.TestMode)
	router := gin.New()
	setupRoutes(router, config)
	return router
}

func decodeJSONResponse[T any](t *testing.T, recorder *httptest.ResponseRecorder) T {
	t.Helper()

	var response T
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析 JSON 响应失败: %v\n响应体: %s", err, recorder.Body.String())
	}
	return response
}

func TestRequestHandlerReturnsRequestAndKubernetesInformation(t *testing.T) {
	router := newTestRouter(diagnosticConfig{
		HTTPPort:       "3000",
		Namespace:      "default",
		Ingress:        "demoapp-ingress",
		Service:        "demoapp-go",
		Pod:            "demoapp-go-69f59978c6-kfw2c",
		TrustedProxies: []string{"10.0.0.0/8"},
	})

	request := httptest.NewRequest(http.MethodGet, "http://example.com/request?a=1&a=2&empty=", nil)
	request.Host = "baidu.com"
	request.Proto = "HTTP/1.1"
	request.ProtoMajor = 1
	request.ProtoMinor = 1
	request.RemoteAddr = "10.0.0.5:54321"
	request.Header.Add("User-Agent", "curl/8.7.1")
	request.Header.Add("X-Test-Value", "first")
	request.Header.Add("X-Test-Value", "second")
	request.Header.Set("X-Forwarded-For", "203.0.113.10")

	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	if contentType := recorder.Header().Get("Content-Type"); contentType != "application/json; charset=utf-8" {
		t.Fatalf("期望 JSON Content-Type，实际为 %q", contentType)
	}
	if !strings.Contains(recorder.Body.String(), "\n    \"path\": \"/request\"") {
		t.Fatalf("/request 响应应默认使用缩进 JSON: %q", recorder.Body.String())
	}

	response := decodeJSONResponse[requestInfoResponse](t, recorder)
	if response.Path != "/request" || response.RequestURI != "/request?a=1&a=2&empty=" {
		t.Fatalf("请求路径信息不正确: path=%q requestURI=%q", response.Path, response.RequestURI)
	}
	if got := response.Query["a"]; len(got) != 2 || got[0] != "1" || got[1] != "2" {
		t.Fatalf("重复查询参数未保留: %#v", got)
	}
	if got := response.Query["empty"]; len(got) != 1 || got[0] != "" {
		t.Fatalf("空查询参数未保留: %#v", got)
	}
	if response.Host != "baidu.com" || response.Method != http.MethodGet || response.Proto != "HTTP/1.1" {
		t.Fatalf("请求行信息不正确: host=%q method=%q proto=%q", response.Host, response.Method, response.Proto)
	}
	if got := response.Headers.Values("X-Test-Value"); len(got) != 2 || got[0] != "first" || got[1] != "second" {
		t.Fatalf("多值请求头未保留: %#v", got)
	}
	if got := response.Headers.Get("User-Agent"); got != "curl/8.7.1" {
		t.Fatalf("User-Agent 不正确: %q", got)
	}
	if response.ClientIP != "203.0.113.10" || response.RemoteIP != "10.0.0.5" {
		t.Fatalf("客户端 IP 信息不正确: clientIP=%q remoteIP=%q", response.ClientIP, response.RemoteIP)
	}
	if response.HTTPPort != "3000" || response.Namespace != "default" || response.Ingress != "demoapp-ingress" || response.Service != "demoapp-go" || response.Pod != "demoapp-go-69f59978c6-kfw2c" {
		t.Fatalf("Kubernetes 元数据不正确: %#v", response)
	}
}

func TestRequestHandlerSupportsCommonHTTPMethodsWithoutEchoingBody(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	methods := []string{
		http.MethodGet,
		http.MethodPost,
		http.MethodPut,
		http.MethodPatch,
		http.MethodDelete,
		http.MethodOptions,
	}

	for _, method := range methods {
		t.Run(method, func(t *testing.T) {
			request := httptest.NewRequest(method, "/request", strings.NewReader("sensitive-body-value"))
			recorder := httptest.NewRecorder()

			router.ServeHTTP(recorder, request)

			if recorder.Code != http.StatusOK {
				t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
			}
			response := decodeJSONResponse[requestInfoResponse](t, recorder)
			if response.Method != method {
				t.Fatalf("期望方法 %q，实际为 %q", method, response.Method)
			}
			if strings.Contains(recorder.Body.String(), "sensitive-body-value") {
				t.Fatal("响应不应回显请求体")
			}
		})
	}
}

func TestRequestHandlerHEADReturnsHeadersWithoutBody(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodHead, "/request", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	if contentType := recorder.Header().Get("Content-Type"); contentType != "application/json; charset=utf-8" {
		t.Fatalf("期望 JSON Content-Type，实际为 %q", contentType)
	}
	payload, err := json.MarshalIndent(newRequestInfoResponse(request, diagnosticConfig{}, remoteIP(request.RemoteAddr)), "", "    ")
	if err != nil {
		t.Fatalf("编码 HEAD 假想响应失败: %v", err)
	}
	if contentLength := recorder.Header().Get("Content-Length"); contentLength != strconv.Itoa(len(payload)) {
		t.Fatalf("HEAD Content-Length=%q，期望 %d", contentLength, len(payload))
	}
	if recorder.Body.Len() != 0 {
		t.Fatalf("HEAD 响应不应包含响应体: %q", recorder.Body.String())
	}
}

func TestIPHandlerReturnsProxyRemotePodAndServiceIPs(t *testing.T) {
	router := newTestRouter(diagnosticConfig{
		PodIP:          "10.244.1.20",
		ServiceIP:      "10.96.10.20",
		TrustedProxies: []string{"2001:db8::/32"},
	})
	request := httptest.NewRequest(http.MethodGet, "/ip", nil)
	request.RemoteAddr = "[2001:db8::5]:54321"
	request.Header.Set("X-Forwarded-For", "203.0.113.10")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	if !strings.Contains(recorder.Body.String(), "\n    \"clientIP\": \"203.0.113.10\"") {
		t.Fatalf("/ip 响应应默认使用缩进 JSON: %q", recorder.Body.String())
	}
	response := decodeJSONResponse[ipInfoResponse](t, recorder)
	if response.ClientIP != "203.0.113.10" || response.RemoteIP != "2001:db8::5" || response.PodIP != "10.244.1.20" || response.ServiceIP != "10.96.10.20" {
		t.Fatalf("IP 信息不正确: %#v", response)
	}
}

func TestIPHandlerReturnsEmptyKubernetesIPsWhenNotConfigured(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/ip", nil)
	request.RemoteAddr = "192.0.2.10:12345"
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	response := decodeJSONResponse[ipInfoResponse](t, recorder)
	if response.PodIP != "" || response.ServiceIP != "" {
		t.Fatalf("未配置的 Kubernetes IP 应返回空字符串: %#v", response)
	}
}

func TestUnregisteredRouteReturnsWelcomeResponse(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	rootRequest := httptest.NewRequest(http.MethodGet, "/", nil)
	rootRequest.Header.Set("User-Agent", "curl/8.7.1")
	rootRecorder := httptest.NewRecorder()
	router.ServeHTTP(rootRecorder, rootRequest)

	unknownRequest := httptest.NewRequest(http.MethodGet, "/not-registered", nil)
	unknownRequest.Header.Set("User-Agent", "curl/8.7.1")
	unknownRecorder := httptest.NewRecorder()
	router.ServeHTTP(unknownRecorder, unknownRequest)

	if unknownRecorder.Code != http.StatusOK {
		t.Fatalf("未注册路径期望状态码 200，实际为 %d", unknownRecorder.Code)
	}
	if unknownRecorder.Body.String() != rootRecorder.Body.String() {
		t.Fatalf("未注册路径应返回 / 的内容:\n/: %q\n未注册路径: %q", rootRecorder.Body.String(), unknownRecorder.Body.String())
	}
}

func TestUnregisteredMethodReturnsWelcomeResponse(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodPost, "/ip", nil)
	request.Header.Set("User-Agent", "curl/8.7.1")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("未注册方法期望状态码 200，实际为 %d", recorder.Code)
	}
	if !strings.Contains(recorder.Body.String(), "Demoapp by vvoo!") {
		t.Fatalf("未注册方法应返回 / 的欢迎内容: %q", recorder.Body.String())
	}
}

func TestLoadDiagnosticConfigReadsEnvironment(t *testing.T) {
	t.Setenv("POD_NAME", "demoapp-go-abc")
	t.Setenv("POD_IP", "10.244.2.30")
	t.Setenv("NAMESPACE", "testing")
	t.Setenv("SERVICE_NAME", "demoapp-go")
	t.Setenv("SERVICE_IP", "10.96.20.30")
	t.Setenv("INGRESS_NAME", "demoapp-ingress")

	config := loadDiagnosticConfig(8080)

	if config.HTTPPort != "8080" || config.Pod != "demoapp-go-abc" || config.PodIP != "10.244.2.30" || config.Namespace != "testing" || config.Service != "demoapp-go" || config.ServiceIP != "10.96.20.30" || config.Ingress != "demoapp-ingress" {
		t.Fatalf("环境变量映射不正确: %#v", config)
	}
}

func TestRequestHandlerDoesNotConsumeBody(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	requestBody := strings.NewReader("body-remains-unread")
	request := httptest.NewRequest(http.MethodPost, "/request", requestBody)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	remainingBody, err := io.ReadAll(request.Body)
	if err != nil {
		t.Fatalf("读取请求体失败: %v", err)
	}
	if string(remainingBody) != "body-remains-unread" {
		t.Fatalf("处理器不应读取请求体，剩余内容为 %q", remainingBody)
	}
}

func TestDelayHandlerUsesSecondsQueryParameter(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/delay?seconds=0", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	var response struct {
		RequestedDelayMs int64  `json:"requested_delay_ms"`
		ActualDelayMs    int64  `json:"actual_delay_ms"`
		Timestamp        string `json:"timestamp"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析延迟响应失败: %v", err)
	}
	if response.RequestedDelayMs != 0 {
		t.Fatalf("seconds=0 应请求 0 毫秒延迟，实际为 %d", response.RequestedDelayMs)
	}
	if response.ActualDelayMs < 0 || response.Timestamp == "" {
		t.Fatalf("延迟响应不完整: %#v", response)
	}
}

func TestDelayHandlerStopsWhenRequestIsCanceled(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/delay?seconds=1", nil)
	ctx, cancel := context.WithCancel(request.Context())
	request = request.WithContext(ctx)
	cancel()
	recorder := httptest.NewRecorder()

	startedAt := time.Now()
	router.ServeHTTP(recorder, request)

	if elapsed := time.Since(startedAt); elapsed > 200*time.Millisecond {
		t.Fatalf("请求取消后延迟处理器仍阻塞了 %v", elapsed)
	}
}

func TestParseDelaySeconds(t *testing.T) {
	tests := []struct {
		name    string
		value   string
		want    time.Duration
		wantErr bool
	}{
		{name: "zero", value: "0", want: 0},
		{name: "five seconds", value: "5", want: 5 * time.Second},
		{name: "maximum", value: "300", want: 300 * time.Second},
		{name: "empty", value: "", wantErr: true},
		{name: "decimal", value: "1.5", wantErr: true},
		{name: "negative", value: "-1", wantErr: true},
		{name: "over maximum", value: "301", wantErr: true},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, err := parseDelaySeconds(test.value)
			if test.wantErr {
				if err == nil {
					t.Fatalf("parseDelaySeconds(%q) 期望返回错误", test.value)
				}
				return
			}
			if err != nil {
				t.Fatalf("parseDelaySeconds(%q) 返回意外错误: %v", test.value, err)
			}
			if got != test.want {
				t.Fatalf("parseDelaySeconds(%q) = %v，期望 %v", test.value, got, test.want)
			}
		})
	}
}

func TestDelayHandlerRejectsInvalidSeconds(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/delay?seconds=301", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusBadRequest {
		t.Fatalf("无效 seconds 期望状态码 400，实际为 %d", recorder.Code)
	}
	var response struct {
		Error struct {
			Code    string `json:"code"`
			Message string `json:"message"`
		} `json:"error"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析错误响应失败: %v", err)
	}
	if response.Error.Code != "INVALID_DELAY_SECONDS" || response.Error.Message == "" {
		t.Fatalf("错误响应不正确: %#v", response)
	}
}

func TestStatusHandlerReturnsRequestedStatusAndInstanceHeaders(t *testing.T) {
	router := newTestRouter(diagnosticConfig{
		Pod:     "demoapp-abc",
		PodIP:   "10.244.1.20",
		Version: "v3.0",
	})
	request := httptest.NewRequest(http.MethodGet, "/status/503", nil)
	request.Header.Set("X-Request-Id", "gateway-request-id")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusServiceUnavailable {
		t.Fatalf("期望状态码 503，实际为 %d", recorder.Code)
	}
	if recorder.Header().Get("X-Demo-Pod") != "demoapp-abc" || recorder.Header().Get("X-Demo-Pod-Ip") != "10.244.1.20" || recorder.Header().Get("X-Demo-Version") != "v3.0" {
		t.Fatalf("实例响应头不完整: %#v", recorder.Header())
	}
	if recorder.Header().Get("X-Demo-Request-Id") != "gateway-request-id" {
		t.Fatalf("请求 ID 未透传: %q", recorder.Header().Get("X-Demo-Request-Id"))
	}
}

func TestStatusHandlerRejectsInvalidStatus(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/status/199", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusBadRequest {
		t.Fatalf("无效状态码期望 400，实际为 %d", recorder.Code)
	}
}

func TestFlakyHandlerFailsConfiguredAttemptsThenSucceeds(t *testing.T) {
	router := newTestRouter(diagnosticConfig{Pod: "demoapp-abc"})

	for attempt := 1; attempt <= 3; attempt++ {
		request := httptest.NewRequest(http.MethodGet, "/flaky?key=retry-case&failures=2&status=503", nil)
		recorder := httptest.NewRecorder()
		router.ServeHTTP(recorder, request)

		wantStatus := http.StatusServiceUnavailable
		if attempt == 3 {
			wantStatus = http.StatusOK
		}
		if recorder.Code != wantStatus {
			t.Fatalf("第 %d 次请求期望状态码 %d，实际为 %d", attempt, wantStatus, recorder.Code)
		}
		var response struct {
			Key     string `json:"key"`
			Attempt int    `json:"attempt"`
			Failed  bool   `json:"failed"`
			Pod     string `json:"pod"`
		}
		if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
			t.Fatalf("解析 flaky 响应失败: %v", err)
		}
		if response.Key != "retry-case" || response.Attempt != attempt || response.Failed != (attempt <= 2) || response.Pod != "demoapp-abc" {
			t.Fatalf("第 %d 次 flaky 响应不正确: %#v", attempt, response)
		}
	}
}

func TestFlakyHandlerLimitsDistinctKeys(t *testing.T) {
	config := diagnosticConfig{}
	state := newAppState(config)
	for index := 0; index < maxFlakyKeys; index++ {
		state.flakyAttempts[strconv.Itoa(index)] = 1
	}
	gin.SetMode(gin.TestMode)
	router := gin.New()
	setupRoutesWithState(router, config, state)

	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/flaky?key=one-more", nil))

	if recorder.Code != http.StatusTooManyRequests {
		t.Fatalf("超过 flaky key 上限期望状态码 429，实际为 %d", recorder.Code)
	}
}

func TestStatsHandlerReportsRequestsByStatus(t *testing.T) {
	config := diagnosticConfig{Pod: "demoapp-abc"}
	state := newAppState(config)
	gin.SetMode(gin.TestMode)
	router := gin.New()
	setupRoutesWithState(router, config, state)

	for _, path := range []string{"/status/201", "/status/503"} {
		recorder := httptest.NewRecorder()
		router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, path, nil))
	}
	statsRecorder := httptest.NewRecorder()
	router.ServeHTTP(statsRecorder, httptest.NewRequest(http.MethodGet, "/stats", nil))

	if statsRecorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", statsRecorder.Code)
	}
	var response statsSnapshot
	if err := json.Unmarshal(statsRecorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析 stats 响应失败: %v", err)
	}
	if response.Pod != "demoapp-abc" || response.TotalRequests < 3 || response.StatusCodes["201"] != 1 || response.StatusCodes["503"] != 1 || response.MaxConcurrency < 1 {
		t.Fatalf("stats 响应不正确: %#v", response)
	}
}

func TestTrustedProxyConfigurationControlsForwardedClientIP(t *testing.T) {
	config := diagnosticConfig{TrustedProxies: []string{"10.0.0.0/8"}}
	router := newTestRouter(config)
	request := httptest.NewRequest(http.MethodGet, "/ip", nil)
	request.RemoteAddr = "10.0.0.5:12345"
	request.Header.Set("X-Forwarded-For", "203.0.113.10")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	response := decodeJSONResponse[ipInfoResponse](t, recorder)
	if response.ClientIP != "203.0.113.10" {
		t.Fatalf("可信代理后的 clientIP 不正确: %q", response.ClientIP)
	}
}

func TestForwardedClientIPIsIgnoredWithoutTrustedProxyConfiguration(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/ip", nil)
	request.RemoteAddr = "10.0.0.5:12345"
	request.Header.Set("X-Forwarded-For", "203.0.113.10")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	response := decodeJSONResponse[ipInfoResponse](t, recorder)
	if response.ClientIP != "10.0.0.5" {
		t.Fatalf("未配置可信代理时不应信任 X-Forwarded-For: %q", response.ClientIP)
	}
}

func TestBeginShutdownRejectsNewWebSockets(t *testing.T) {
	state := newAppState(diagnosticConfig{})
	state.beginShutdown()

	if state.registerWebSocket(nil) {
		t.Fatal("关闭开始后不应再登记新的 WebSocket 连接")
	}
}
