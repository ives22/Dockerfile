docker buildx build \
  --platform=linux/amd64,linux/arm64,linux/arm \
  -t vvoo/demoapp:v2.1 \
  -t vvoo/demoapp:latest \
  --push \
  .
