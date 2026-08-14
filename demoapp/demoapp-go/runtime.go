package main

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"log"
	"net/http"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

const maxFlakyKeys = 10_000

type appState struct {
	mu sync.RWMutex

	config          diagnosticConfig
	livezStatus     string
	readyzStatus    string
	shuttingDown    bool
	flakyAttempts   map[string]int
	totalRequests   uint64
	statusCodes     map[string]uint64
	currentRequests int64
	maxConcurrency  int64
	startedAt       time.Time
	webSockets      map[*websocket.Conn]struct{}
}

type statsSnapshot struct {
	Pod             string            `json:"pod"`
	PodIP           string            `json:"podIP"`
	Version         string            `json:"version"`
	StartedAt       string            `json:"startedAt"`
	UptimeSeconds   int64             `json:"uptimeSeconds"`
	TotalRequests   uint64            `json:"totalRequests"`
	StatusCodes     map[string]uint64 `json:"statusCodes"`
	CurrentRequests int64             `json:"currentRequests"`
	MaxConcurrency  int64             `json:"maxConcurrency"`
	ShuttingDown    bool              `json:"shuttingDown"`
}

func newAppState(config diagnosticConfig) *appState {
	return &appState{
		config:        config,
		livezStatus:   "OK",
		readyzStatus:  "OK",
		flakyAttempts: make(map[string]int),
		statusCodes:   make(map[string]uint64),
		startedAt:     time.Now(),
		webSockets:    make(map[*websocket.Conn]struct{}),
	}
}

func (s *appState) beginRequest() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.totalRequests++
	s.currentRequests++
	if s.currentRequests > s.maxConcurrency {
		s.maxConcurrency = s.currentRequests
	}
}

func (s *appState) finishRequest(status int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.currentRequests--
	s.statusCodes[strconv.Itoa(status)]++
}

func (s *appState) nextFlakyAttempt(key string) (int, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.flakyAttempts[key]; !exists && len(s.flakyAttempts) >= maxFlakyKeys {
		return 0, false
	}
	s.flakyAttempts[key]++
	return s.flakyAttempts[key], true
}

func (s *appState) health(statusType string) string {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if statusType == "livez" {
		return s.livezStatus
	}
	return s.readyzStatus
}

func (s *appState) setHealth(statusType, value string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if statusType == "livez" {
		s.livezStatus = value
		return
	}
	s.readyzStatus = value
}

func (s *appState) beginShutdown() {
	s.mu.Lock()
	s.shuttingDown = true
	s.readyzStatus = "SHUTTING_DOWN"
	connections := make([]*websocket.Conn, 0, len(s.webSockets))
	for connection := range s.webSockets {
		connections = append(connections, connection)
	}
	s.mu.Unlock()

	deadline := time.Now().Add(time.Second)
	for _, connection := range connections {
		_ = connection.WriteControl(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseGoingAway, "server shutting down"), deadline)
		_ = connection.Close()
	}
}

func (s *appState) registerWebSocket(connection *websocket.Conn) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.shuttingDown {
		return false
	}
	s.webSockets[connection] = struct{}{}
	return true
}

func (s *appState) unregisterWebSocket(connection *websocket.Conn) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.webSockets, connection)
}

func (s *appState) snapshot() statsSnapshot {
	s.mu.RLock()
	defer s.mu.RUnlock()

	statusCodes := make(map[string]uint64, len(s.statusCodes))
	for status, count := range s.statusCodes {
		statusCodes[status] = count
	}

	return statsSnapshot{
		Pod:             s.config.Pod,
		PodIP:           s.config.PodIP,
		Version:         s.config.Version,
		StartedAt:       s.startedAt.Format(time.RFC3339),
		UptimeSeconds:   int64(time.Since(s.startedAt).Seconds()),
		TotalRequests:   s.totalRequests,
		StatusCodes:     statusCodes,
		CurrentRequests: s.currentRequests,
		MaxConcurrency:  s.maxConcurrency,
		ShuttingDown:    s.shuttingDown,
	}
}

func requestID(header http.Header) string {
	if value := header.Get("X-Request-Id"); value != "" {
		return value
	}
	if value := header.Get("Traceparent"); value != "" {
		return value
	}

	buffer := make([]byte, 16)
	if _, err := rand.Read(buffer); err != nil {
		return fmt.Sprintf("fallback-%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buffer)
}

func diagnosticsMiddleware(state *appState) gin.HandlerFunc {
	return func(c *gin.Context) {
		startTime := time.Now()
		id := requestID(c.Request.Header)
		c.Set("requestID", id)
		c.Header("X-Demo-Request-Id", id)
		c.Header("X-Demo-Pod", state.config.Pod)
		c.Header("X-Demo-Pod-Ip", state.config.PodIP)
		c.Header("X-Demo-Version", state.config.Version)
		state.beginRequest()

		c.Next()

		statusCode := c.Writer.Status()
		state.finishRequest(statusCode)
		path := c.Request.URL.RequestURI()
		log.Printf("[GIN] %s | %3d | %13v | %15s | %-7s %s | request_id=%s pod=%s",
			time.Now().Format("2006/01/02 - 15:04:05"),
			statusCode,
			time.Since(startTime),
			c.ClientIP(),
			c.Request.Method,
			path,
			id,
			state.config.Pod,
		)
	}
}
