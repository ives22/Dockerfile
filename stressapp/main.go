package main

import (
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"runtime"
	"strconv"
	"sync"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

var (
	// Prometheus metrics
	cpuLoad = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "app_cpu_load_percent",
		Help: "Current CPU load percentage",
	})
	memoryUsage = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "app_memory_usage_bytes",
		Help: "Current memory usage in bytes",
	})
	requestsTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "app_requests_total",
		Help: "Total number of requests",
	})

	// 探针状态控制变量
	healthzStatus = true
	readyzStatus  = true
	probeMutex    sync.RWMutex
)

func init() {
	// Register metrics with Prometheus
	prometheus.MustRegister(cpuLoad)
	prometheus.MustRegister(memoryUsage)
	prometheus.MustRegister(requestsTotal)
}

func main() {
	http.HandleFunc("/cpu", cpuIntensiveHandler)
	http.HandleFunc("/memory", memoryIntensiveHandler)
	http.HandleFunc("/combined", combinedLoadHandler)
	http.HandleFunc("/diskio", diskIOHandler)
	http.HandleFunc("/metrics", promhttp.Handler().ServeHTTP)
	http.HandleFunc("/", normalHandler)
	http.HandleFunc("/healthz", healthzHandler)
	http.HandleFunc("/readyz", readyzHandler)
	// 新增探针状态控制接口
	http.HandleFunc("/probe/healthz/set", setHealthzHandler)
	http.HandleFunc("/probe/readyz/set", setReadyzHandler)
	http.HandleFunc("/probe/status", probeStatusHandler)

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	fmt.Printf("Server starting on port %s...\n", port)
	if err := http.ListenAndServe(":"+port, nil); err != nil {
		panic(err)
	}
}

// CPU intensive handler
func cpuIntensiveHandler(w http.ResponseWriter, r *http.Request) {
	requestsTotal.Inc()

	// 获取时长参数
	secondsStr := r.URL.Query().Get("seconds")
	seconds, err := strconv.Atoi(secondsStr)
	if err != nil || seconds <= 0 {
		seconds = 10
	}

	// 获取 CPU 使用率参数
	intensityStr := r.URL.Query().Get("intensity")
	intensity, err := strconv.Atoi(intensityStr)
	if err != nil || intensity < 1 || intensity > 100 {
		intensity = 100
	}

	go func() {
		defer cpuLoad.Set(0) // 任务完成后清 0
		cpuLoad.Set(float64(intensity))

		duration := time.Duration(seconds) * time.Second
		stop := time.After(duration)
		cycle := 100 * time.Millisecond
		busyTime := time.Duration(float64(cycle) * float64(intensity) / 100.0)
		idleTime := cycle - busyTime

		for {
			select {
			case <-stop:
				return
			default:
				start := time.Now()
				// 忙等待 busyTime
				for time.Since(start) < busyTime {
					_ = math.Sqrt(12345.6789) // 一点浮点运算模拟 CPU 消耗
				}
				// 休眠 idleTime
				time.Sleep(idleTime)
			}
		}
	}()

	// 返回响应
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":   "ok",
		"message":  fmt.Sprintf("CPU load started: %ds at %d%%", seconds, intensity),
		"duration": fmt.Sprintf("%ds", seconds),
	})
}

// Memory intensive handler
func memoryIntensiveHandler(w http.ResponseWriter, r *http.Request) {
	requestsTotal.Inc()

	mbStr := r.URL.Query().Get("mb")
	mb, err := strconv.Atoi(mbStr)
	if err != nil || mb <= 0 {
		mb = 100
	}

	durationStr := r.URL.Query().Get("seconds")
	duration, err := strconv.Atoi(durationStr)
	if err != nil || duration <= 0 {
		duration = 30
	}

	// Allocate memory
	data := make([]byte, mb*1024*1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	memoryUsage.Set(float64(mb * 1024 * 1024))

	// Release memory after duration
	go func() {
		time.Sleep(time.Duration(duration) * time.Second)
		data = nil
		runtime.GC()
		memoryUsage.Set(0)
	}()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":   "ok",
		"message":  fmt.Sprintf("Allocated %d MB of memory for %d seconds", mb, duration),
		"duration": fmt.Sprintf("%ds", duration),
	})
}

// Combined CPU and memory intensive handler
func combinedLoadHandler(w http.ResponseWriter, r *http.Request) {
	requestsTotal.Inc()

	// Get parameters
	cpuSecondsStr := r.URL.Query().Get("cpu_seconds")
	cpuSeconds, err := strconv.Atoi(cpuSecondsStr)
	if err != nil || cpuSeconds <= 0 {
		cpuSeconds = 10
	}

	cpuIntensityStr := r.URL.Query().Get("cpu_intensity")
	cpuIntensity, err := strconv.Atoi(cpuIntensityStr)
	if err != nil || cpuIntensity <= 0 {
		cpuIntensity = 50
	}

	memoryMBStr := r.URL.Query().Get("memory_mb")
	memoryMB, err := strconv.Atoi(memoryMBStr)
	if err != nil || memoryMB <= 0 {
		memoryMB = 50
	}

	memorySecondsStr := r.URL.Query().Get("memory_seconds")
	memorySeconds, err := strconv.Atoi(memorySecondsStr)
	if err != nil || memorySeconds <= 0 {
		memorySeconds = 30
	}

	// Start CPU load
	go func() {
		start := time.Now()
		for time.Since(start) < time.Duration(cpuSeconds)*time.Second {
			for i := 0; i < cpuIntensity*1000000; i++ {
				_ = math.Sqrt(float64(i))
			}
			cpuLoad.Set(float64(cpuIntensity))
			runtime.Gosched()
		}
		cpuLoad.Set(0)
	}()

	// Start memory load
	go func() {
		data := make([]byte, memoryMB*1024*1024)
		for i := range data {
			data[i] = byte(i % 256)
		}

		memoryUsage.Set(float64(memoryMB * 1024 * 1024))

		time.Sleep(time.Duration(memorySeconds) * time.Second)
		data = nil
		runtime.GC()
		memoryUsage.Set(0)
	}()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":   "ok",
		"message":  fmt.Sprintf("Started combined load: CPU %d%% for %ds, Memory %dMB for %ds", cpuIntensity, cpuSeconds, memoryMB, memorySeconds),
		"duration": fmt.Sprintf("CPU: %ds, Memory: %ds", cpuSeconds, memorySeconds),
	})
}

// Disk IO intensive handler
func diskIOHandler(w http.ResponseWriter, r *http.Request) {
	requestsTotal.Inc()

	sizeMBStr := r.URL.Query().Get("mb")
	sizeMB, err := strconv.Atoi(sizeMBStr)
	if err != nil || sizeMB <= 0 {
		sizeMB = 100
	}

	iterationsStr := r.URL.Query().Get("iterations")
	iterations, err := strconv.Atoi(iterationsStr)
	if err != nil || iterations <= 0 {
		iterations = 10
	}

	go func() {
		tmpFile := "/tmp/diskio_test.dat"
		defer os.Remove(tmpFile)

		data := make([]byte, 1024*1024) // 1MB chunk
		for i := range data {
			data[i] = byte(i % 256)
		}

		for i := 0; i < iterations; i++ {
			// Write file
			f, err := os.Create(tmpFile)
			if err != nil {
				return
			}

			for j := 0; j < sizeMB; j++ {
				if _, err := f.Write(data); err != nil {
					f.Close()
					return
				}
			}
			f.Close()

			// Read file
			f, err = os.Open(tmpFile)
			if err != nil {
				return
			}

			buf := make([]byte, 1024)
			for {
				_, err := f.Read(buf)
				if err == io.EOF {
					break
				}
			}
			f.Close()
		}
	}()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":     "ok",
		"message":    fmt.Sprintf("Disk IO intensive task started: %d MB, %d iterations", sizeMB, iterations),
		"size_mb":    fmt.Sprintf("%d", sizeMB),
		"iterations": fmt.Sprintf("%d", iterations),
	})
}

// Normal handler
func normalHandler(w http.ResponseWriter, r *http.Request) {
	requestsTotal.Inc()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":  "ok",
		"message": "Normal request processed",
		"time":    time.Now().Format(time.RFC3339),
	})
}

// 存活探针（动态控制）
func healthzHandler(w http.ResponseWriter, r *http.Request) {
	probeMutex.RLock()
	defer probeMutex.RUnlock()

	if healthzStatus {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("OK"))
	} else {
		w.WriteHeader(http.StatusServiceUnavailable)
		w.Write([]byte("Unhealthy"))
	}
}

// 就绪探针（动态控制）
func readyzHandler(w http.ResponseWriter, r *http.Request) {
	probeMutex.RLock()
	defer probeMutex.RUnlock()

	if readyzStatus {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("OK"))
	} else {
		w.WriteHeader(http.StatusServiceUnavailable)
		w.Write([]byte("Not Ready"))
	}
}

// 设置存活探针状态
func setHealthzHandler(w http.ResponseWriter, r *http.Request) {
	status := r.URL.Query().Get("status")
	probeMutex.Lock()
	defer probeMutex.Unlock()

	if status == "ok" {
		healthzStatus = true
		w.Write([]byte("Healthz status set to OK"))
	} else {
		healthzStatus = false
		w.Write([]byte("Healthz status set to Unhealthy"))
	}
}

// 设置就绪探针状态
func setReadyzHandler(w http.ResponseWriter, r *http.Request) {
	status := r.URL.Query().Get("status")
	probeMutex.Lock()
	defer probeMutex.Unlock()

	if status == "ok" {
		readyzStatus = true
		w.Write([]byte("Readyz status set to OK"))
	} else {
		readyzStatus = false
		w.Write([]byte("Readyz status set to Not Ready"))
	}
}

// 查看探针当前状态
func probeStatusHandler(w http.ResponseWriter, r *http.Request) {
	probeMutex.RLock()
	defer probeMutex.RUnlock()

	json.NewEncoder(w).Encode(map[string]bool{
		"healthz": healthzStatus,
		"readyz":  readyzStatus,
	})
}
