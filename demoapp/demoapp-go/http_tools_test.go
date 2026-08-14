package main

import (
	"bufio"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

func TestEchoHandlerReturnsBodyAndMetadata(t *testing.T) {
	router := newTestRouter(diagnosticConfig{Pod: "demoapp-abc"})
	request := httptest.NewRequest(http.MethodPost, "/echo", strings.NewReader("hello gateway"))
	request.Header.Set("Content-Type", "text/plain")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	var response struct {
		Body        string `json:"body"`
		Size        int    `json:"size"`
		ContentType string `json:"contentType"`
		Pod         string `json:"pod"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析 echo 响应失败: %v", err)
	}
	if response.Body != "hello gateway" || response.Size != 13 || response.ContentType != "text/plain" || response.Pod != "demoapp-abc" {
		t.Fatalf("echo 响应不正确: %#v", response)
	}
}

func TestEchoHandlerRejectsOversizedBody(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodPost, "/echo", strings.NewReader(strings.Repeat("a", maxEchoBodyBytes+1)))
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusRequestEntityTooLarge {
		t.Fatalf("超大请求体期望 413，实际为 %d", recorder.Code)
	}
}

func TestStreamHandlerWritesConfiguredChunks(t *testing.T) {
	router := newTestRouter(diagnosticConfig{Pod: "demoapp-abc"})
	request := httptest.NewRequest(http.MethodGet, "/stream?chunks=3&intervalMs=0", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	scanner := bufio.NewScanner(strings.NewReader(recorder.Body.String()))
	lines := 0
	for scanner.Scan() {
		lines++
	}
	if lines != 3 {
		t.Fatalf("期望 3 个数据块，实际为 %d: %q", lines, recorder.Body.String())
	}
}

func TestBytesHandlerReturnsRequestedSize(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/bytes?size=1024", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK || recorder.Body.Len() != 1024 {
		t.Fatalf("期望 1024 字节和状态码 200，实际为 size=%d status=%d", recorder.Body.Len(), recorder.Code)
	}
	if recorder.Header().Get("Content-Length") != "1024" {
		t.Fatalf("Content-Length 不正确: %q", recorder.Header().Get("Content-Length"))
	}
}

func TestCookieHandlerSetsAndReadsCookie(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	setRecorder := httptest.NewRecorder()
	router.ServeHTTP(setRecorder, httptest.NewRequest(http.MethodGet, "/cookie?action=set&name=route&value=pod-a", nil))
	setResponse := setRecorder.Result()
	cookies := setResponse.Cookies()
	if setRecorder.Code != http.StatusOK || len(cookies) != 1 || cookies[0].Name != "route" || cookies[0].Value != "pod-a" {
		t.Fatalf("设置 Cookie 失败: status=%d cookies=%#v", setRecorder.Code, cookies)
	}

	readRequest := httptest.NewRequest(http.MethodGet, "/cookie?action=read&name=route", nil)
	readRequest.AddCookie(cookies[0])
	readRecorder := httptest.NewRecorder()
	router.ServeHTTP(readRecorder, readRequest)
	if !strings.Contains(readRecorder.Body.String(), `"value":"pod-a"`) {
		t.Fatalf("读取 Cookie 失败: %q", readRecorder.Body.String())
	}
}

func TestWildcardRequestHandlerShowsRewrittenPath(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodGet, "/request/origin/fullpath?x=1", nil)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, request)

	response := decodeJSONResponse[requestInfoResponse](t, recorder)
	if response.Path != "/request/origin/fullpath" || response.RequestURI != "/request/origin/fullpath?x=1" {
		t.Fatalf("通配请求路径不正确: %#v", response)
	}
}

func TestMetricsHandlerExposesDemoMetrics(t *testing.T) {
	router := newTestRouter(diagnosticConfig{Pod: "demoapp-abc"})
	router.ServeHTTP(httptest.NewRecorder(), httptest.NewRequest(http.MethodGet, "/status/201", nil))
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/metrics", nil))

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	body := recorder.Body.String()
	for _, metric := range []string{"demoapp_http_requests_total", "demoapp_http_current_requests", "demoapp_http_max_concurrency"} {
		if !strings.Contains(body, metric) {
			t.Fatalf("指标 %q 不存在: %s", metric, body)
		}
	}
}

func TestWebSocketHandlerEchoesMessages(t *testing.T) {
	router := newTestRouter(diagnosticConfig{Pod: "demoapp-abc"})
	server := httptest.NewServer(router)
	defer server.Close()

	url := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws"
	connection, response, err := websocket.DefaultDialer.Dial(url, nil)
	if err != nil {
		if response != nil {
			t.Fatalf("连接 WebSocket 失败: %v status=%d", err, response.StatusCode)
		}
		t.Fatalf("连接 WebSocket 失败: %v", err)
	}
	defer connection.Close()
	connection.SetReadDeadline(time.Now().Add(2 * time.Second))

	if err := connection.WriteMessage(websocket.TextMessage, []byte("hello")); err != nil {
		t.Fatalf("写入 WebSocket 失败: %v", err)
	}
	messageType, message, err := connection.ReadMessage()
	if err != nil {
		t.Fatalf("读取 WebSocket 失败: %v", err)
	}
	if messageType != websocket.TextMessage || string(message) != "hello" {
		t.Fatalf("WebSocket Echo 不正确: type=%d message=%q", messageType, message)
	}
}

func TestStreamParametersRejectUnsafeValues(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	for _, path := range []string{"/stream?chunks=101", "/stream?intervalMs=10001", "/bytes?size=10485761"} {
		recorder := httptest.NewRecorder()
		router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, path, nil))
		if recorder.Code != http.StatusBadRequest {
			t.Fatalf("%s 期望状态码 400，实际为 %d", path, recorder.Code)
		}
	}
}

func TestEchoAcceptsBinaryBodyAsBase64(t *testing.T) {
	router := newTestRouter(diagnosticConfig{})
	request := httptest.NewRequest(http.MethodPost, "/echo", strings.NewReader("\x00\x01"))
	request.Header.Set("Content-Type", "application/octet-stream")
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("期望状态码 200，实际为 %d", recorder.Code)
	}
	var response struct {
		Body     string `json:"body"`
		Encoding string `json:"encoding"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatalf("解析二进制 echo 响应失败: %v", err)
	}
	if response.Encoding != "base64" || response.Body != "AAE=" {
		t.Fatalf("二进制响应应使用 base64: %#v", response)
	}
}
