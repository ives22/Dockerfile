package main

import (
	"fmt"
	"log"
	"net/http"
	"sync/atomic"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

var (
	requestCount uint64

	requests = prometheus.NewGaugeFunc(prometheus.GaugeOpts{
		Name: "custom_requests_total",
		Help: "Total number of custom requests handled",
	}, func() float64 {
		return float64(atomic.LoadUint64(&requestCount))
	})
)

func init() {
	prometheus.MustRegister(requests)
}

func handler(w http.ResponseWriter, r *http.Request) {
	atomic.AddUint64(&requestCount, 1)
	fmt.Fprintln(w, "OK")
}

func main() {
	http.HandleFunc("/", handler)
	http.Handle("/metrics", promhttp.Handler())

	fmt.Println("Listening on :80")
	log.Fatal(http.ListenAndServe(":80", nil))
}
