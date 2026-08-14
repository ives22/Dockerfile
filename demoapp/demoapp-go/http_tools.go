package main

import (
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

const (
	maxEchoBodyBytes  = 1 << 20
	maxResponseBytes  = 10 << 20
	maxStreamChunks   = 100
	maxStreamInterval = 10 * time.Second
)

func registerHTTPToolRoutes(router *gin.Engine, config diagnosticConfig, state *appState) {
	router.POST("/echo", echoHandler(config))
	router.GET("/stream", streamHandler(config))
	router.GET("/bytes", bytesHandler)
	router.GET("/cookie", cookieHandler)

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
		router.Handle(method, "/request/*path", requestInfoHandler(config))
	}

	router.GET("/ws", websocketHandler(state))
	router.GET("/metrics", gin.WrapH(metricsHandler(state)))
}

func echoHandler(config diagnosticConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		body, err := io.ReadAll(io.LimitReader(c.Request.Body, maxEchoBodyBytes+1))
		if err != nil {
			c.JSON(http.StatusBadRequest, apiError("INVALID_BODY", "无法读取请求体"))
			return
		}
		if len(body) > maxEchoBodyBytes {
			c.JSON(http.StatusRequestEntityTooLarge, apiError("BODY_TOO_LARGE", "请求体不能超过 1 MiB"))
			return
		}

		encoding := "utf-8"
		bodyValue := string(body)
		if !isSafeText(body) {
			encoding = "base64"
			bodyValue = base64.StdEncoding.EncodeToString(body)
		}

		c.IndentedJSON(http.StatusOK, gin.H{
			"body":        bodyValue,
			"size":        len(body),
			"encoding":    encoding,
			"contentType": c.GetHeader("Content-Type"),
			"pod":         config.Pod,
			"requestID":   c.GetString("requestID"),
		})
	}
}

func isSafeText(body []byte) bool {
	if !utf8.Valid(body) {
		return false
	}
	for _, character := range string(body) {
		if character < 0x20 && character != '\n' && character != '\r' && character != '\t' {
			return false
		}
	}
	return true
}

func streamHandler(config diagnosticConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		chunks, err := boundedIntQuery(c, "chunks", 5, 1, maxStreamChunks)
		if err != nil {
			c.JSON(http.StatusBadRequest, apiError("INVALID_STREAM_CONFIG", err.Error()))
			return
		}
		intervalMs, err := boundedIntQuery(c, "intervalMs", 1000, 0, int(maxStreamInterval/time.Millisecond))
		if err != nil {
			c.JSON(http.StatusBadRequest, apiError("INVALID_STREAM_CONFIG", err.Error()))
			return
		}

		c.Header("Content-Type", "application/x-ndjson")
		c.Header("Cache-Control", "no-cache")
		c.Header("X-Accel-Buffering", "no")
		for chunk := 1; chunk <= chunks; chunk++ {
			select {
			case <-c.Request.Context().Done():
				return
			default:
			}
			_, _ = fmt.Fprintf(c.Writer, "{\"chunk\":%d,\"total\":%d,\"pod\":%q,\"timestamp\":%q}\n", chunk, chunks, config.Pod, time.Now().Format(time.RFC3339Nano))
			c.Writer.Flush()
			if chunk < chunks && !waitForRequest(c.Request.Context().Done(), time.Duration(intervalMs)*time.Millisecond) {
				return
			}
		}
	}
}

func bytesHandler(c *gin.Context) {
	size, err := boundedIntQuery(c, "size", 1024, 0, maxResponseBytes)
	if err != nil {
		c.JSON(http.StatusBadRequest, apiError("INVALID_BYTE_SIZE", err.Error()))
		return
	}
	c.Header("Content-Type", "application/octet-stream")
	c.Header("Content-Length", strconv.Itoa(size))
	c.Status(http.StatusOK)
	block := []byte(strings.Repeat("a", 32*1024))
	remaining := size
	for remaining > 0 {
		writeSize := min(remaining, len(block))
		if _, err := c.Writer.Write(block[:writeSize]); err != nil {
			return
		}
		remaining -= writeSize
	}
}

func cookieHandler(c *gin.Context) {
	name := strings.TrimSpace(c.DefaultQuery("name", "demoapp-route"))
	if name == "" || len(name) > 128 || strings.ContainsAny(name, " ;,\t\r\n") {
		c.JSON(http.StatusBadRequest, apiError("INVALID_COOKIE_NAME", "Cookie 名称无效"))
		return
	}

	switch c.DefaultQuery("action", "read") {
	case "set":
		value := c.Query("value")
		if len(value) > 1024 {
			c.JSON(http.StatusBadRequest, apiError("INVALID_COOKIE_VALUE", "Cookie 值不能超过 1024 字节"))
			return
		}
		http.SetCookie(c.Writer, &http.Cookie{Name: name, Value: value, Path: "/", HttpOnly: true, SameSite: http.SameSiteLaxMode})
		c.JSON(http.StatusOK, gin.H{"action": "set", "name": name, "value": value})
	case "read":
		cookie, err := c.Request.Cookie(name)
		if errors.Is(err, http.ErrNoCookie) {
			c.JSON(http.StatusOK, gin.H{"action": "read", "name": name, "value": "", "found": false})
			return
		}
		if err != nil {
			c.JSON(http.StatusBadRequest, apiError("INVALID_COOKIE", "Cookie 无法解析"))
			return
		}
		c.JSON(http.StatusOK, gin.H{"action": "read", "name": name, "value": cookie.Value, "found": true})
	case "delete":
		http.SetCookie(c.Writer, &http.Cookie{Name: name, Path: "/", MaxAge: -1, HttpOnly: true, SameSite: http.SameSiteLaxMode})
		c.JSON(http.StatusOK, gin.H{"action": "delete", "name": name})
	default:
		c.JSON(http.StatusBadRequest, apiError("INVALID_COOKIE_ACTION", "action 必须是 set、read 或 delete"))
	}
}

var websocketUpgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
}

func websocketHandler(state *appState) gin.HandlerFunc {
	return func(c *gin.Context) {
		connection, err := websocketUpgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			return
		}
		if !state.registerWebSocket(connection) {
			_ = connection.WriteControl(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseGoingAway, "server shutting down"), time.Now().Add(time.Second))
			_ = connection.Close()
			return
		}
		defer func() {
			state.unregisterWebSocket(connection)
			_ = connection.Close()
		}()
		connection.SetReadLimit(maxEchoBodyBytes)
		for {
			messageType, message, err := connection.ReadMessage()
			if err != nil {
				return
			}
			if err := connection.WriteMessage(messageType, message); err != nil {
				return
			}
		}
	}
}

func metricsHandler(state *appState) http.Handler {
	registry := prometheus.NewRegistry()
	registry.MustRegister(prometheus.NewGoCollector())
	registry.MustRegister(&stateCollector{state: state})
	return promhttp.HandlerFor(registry, promhttp.HandlerOpts{})
}

type stateCollector struct {
	state *appState
}

var (
	totalRequestsMetric   = prometheus.NewDesc("demoapp_http_requests_total", "Total HTTP requests handled by this pod.", []string{"pod", "status"}, nil)
	currentRequestsMetric = prometheus.NewDesc("demoapp_http_current_requests", "Current HTTP requests in progress.", []string{"pod"}, nil)
	maxConcurrencyMetric  = prometheus.NewDesc("demoapp_http_max_concurrency", "Maximum observed HTTP request concurrency.", []string{"pod"}, nil)
)

func (collector *stateCollector) Describe(channel chan<- *prometheus.Desc) {
	channel <- totalRequestsMetric
	channel <- currentRequestsMetric
	channel <- maxConcurrencyMetric
}

func (collector *stateCollector) Collect(channel chan<- prometheus.Metric) {
	snapshot := collector.state.snapshot()
	for status, count := range snapshot.StatusCodes {
		channel <- prometheus.MustNewConstMetric(totalRequestsMetric, prometheus.CounterValue, float64(count), snapshot.Pod, status)
	}
	channel <- prometheus.MustNewConstMetric(currentRequestsMetric, prometheus.GaugeValue, float64(snapshot.CurrentRequests), snapshot.Pod)
	channel <- prometheus.MustNewConstMetric(maxConcurrencyMetric, prometheus.GaugeValue, float64(snapshot.MaxConcurrency), snapshot.Pod)
}

func boundedIntQuery(c *gin.Context, name string, defaultValue, minimum, maximum int) (int, error) {
	value := c.DefaultQuery(name, strconv.Itoa(defaultValue))
	parsed, err := strconv.Atoi(value)
	if err != nil || parsed < minimum || parsed > maximum {
		return 0, fmt.Errorf("%s 必须是 %d 到 %d 之间的整数", name, minimum, maximum)
	}
	return parsed, nil
}

func apiError(code, message string) gin.H {
	return gin.H{"error": gin.H{"code": code, "message": message}}
}

func waitForRequest(done <-chan struct{}, duration time.Duration) bool {
	timer := time.NewTimer(duration)
	defer timer.Stop()
	select {
	case <-timer.C:
		return true
	case <-done:
		return false
	}
}
