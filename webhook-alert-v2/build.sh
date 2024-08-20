#!/bin/bash
docker buildx build -t vvoo/webhook-alert:$1 --platform=linux/amd64,linux/arm64,linux/arm . --push
