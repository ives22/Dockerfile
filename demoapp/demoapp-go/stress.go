package main

import (
	"crypto/sha256"
	"crypto/subtle"
	"net/http"
	"runtime"
	"time"

	"github.com/gin-gonic/gin"
)

const (
	maxCPUStressMilliseconds = 10_000
	maxMemoryStressMegabytes = 256
	maxMemoryHoldSeconds     = 60
)

func registerStressRoutes(router *gin.Engine, config diagnosticConfig) {
	if config.StressToken == "" {
		router.GET("/stress/cpu", disabledStressHandler)
		router.GET("/stress/memory", disabledStressHandler)
		return
	}
	group := router.Group("/stress", stressTokenMiddleware(config.StressToken))
	group.GET("/cpu", cpuStressHandler)
	group.GET("/memory", memoryStressHandler)
}

func disabledStressHandler(c *gin.Context) {
	c.JSON(http.StatusNotFound, apiError("STRESS_DISABLED", "压力测试接口未启用"))
}

func stressTokenMiddleware(expected string) gin.HandlerFunc {
	expectedHash := sha256.Sum256([]byte(expected))
	return func(c *gin.Context) {
		providedHash := sha256.Sum256([]byte(c.GetHeader("X-Stress-Token")))
		if subtle.ConstantTimeCompare(expectedHash[:], providedHash[:]) != 1 {
			c.AbortWithStatusJSON(http.StatusUnauthorized, apiError("INVALID_STRESS_TOKEN", "压力测试 Token 无效"))
			return
		}
		c.Next()
	}
}

func cpuStressHandler(c *gin.Context) {
	milliseconds, err := boundedIntQuery(c, "milliseconds", 1000, 1, maxCPUStressMilliseconds)
	if err != nil {
		c.JSON(http.StatusBadRequest, apiError("INVALID_CPU_STRESS", err.Error()))
		return
	}
	deadline := time.Now().Add(time.Duration(milliseconds) * time.Millisecond)
	var iterations uint64
	for time.Now().Before(deadline) {
		iterations++
		if iterations%100_000 == 0 {
			select {
			case <-c.Request.Context().Done():
				return
			default:
			}
		}
	}
	c.JSON(http.StatusOK, gin.H{"milliseconds": milliseconds, "iterations": iterations})
}

func memoryStressHandler(c *gin.Context) {
	megabytes, err := boundedIntQuery(c, "megabytes", 16, 1, maxMemoryStressMegabytes)
	if err != nil {
		c.JSON(http.StatusBadRequest, apiError("INVALID_MEMORY_STRESS", err.Error()))
		return
	}
	holdSeconds, err := boundedIntQuery(c, "holdSeconds", 5, 0, maxMemoryHoldSeconds)
	if err != nil {
		c.JSON(http.StatusBadRequest, apiError("INVALID_MEMORY_STRESS", err.Error()))
		return
	}

	memory := make([]byte, megabytes<<20)
	for offset := 0; offset < len(memory); offset += 4096 {
		memory[offset] = byte(offset)
	}
	select {
	case <-time.After(time.Duration(holdSeconds) * time.Second):
	case <-c.Request.Context().Done():
		return
	}
	runtime.KeepAlive(memory)
	c.JSON(http.StatusOK, gin.H{"megabytes": megabytes, "holdSeconds": holdSeconds})
}
