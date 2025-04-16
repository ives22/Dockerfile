package main

import (
	"context"
	pb "demoapp/data"
	"flag"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"log"
	"time"
)

var (
	serverAddr = flag.String("addr", "ingress-demo.demo.com:80", "The server address in the format of host:port")
	clientID   = flag.String("client", "test-client", "The client ID")
)

func main() {
	flag.Parse()

	conn, err := grpc.Dial(*serverAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()

	c := pb.NewDemoClient(conn)

	ctx := context.Background()

	for {
		stream, err := c.SayHello(ctx, &pb.HelloRequest{Name: *clientID})
		if err != nil {
			log.Fatalf("could not connect: %v", err)
		}
		time.Sleep(2 * time.Second)
		log.Printf(stream.Message)
	}
}
