# 基于nginx制作的base镜像，用于添加一些特定的环境变量
# FROM nginx:1.26.2

# # 时区环境变量
# ENV TZ=UTC

# # 其它自定义环境变量
# ENV VITE_APP_API_URL=/api   
# ENV VITE_APP_ENV=dev    
# ENV VITE_APP_VERSION=v1.0.0 

# RUN ln -sf /usr/share/zoneinfo/$TZ /etc/localtime && \
#     echo "$TZ" > /etc/timezone

# # RUN sed -i '9a try_files \$uri \$uri/ /index.html;' /etc/nginx/conf.d/default.conf


# CMD ["nginx", "-g", "daemon off;"]


# 选择官方稳定版本作为基础镜像
FROM nginx:1.26.2

# 设置作者信息（可选）
LABEL maintainer="lucky@games.com" \
      description="Nginx base image with custom environment variables" \
      version="1.0.0"

# 设置默认时区和应用相关环境变量
ENV TZ=UTC \
    VITE_APP_ENV=dev 

# 设置系统时区
RUN ln -snf /usr/share/zoneinfo/${TZ} /etc/localtime && \
    echo "${TZ}" > /etc/timezone

# 复制默认 nginx 配置
COPY ./nginx.conf /etc/nginx/conf.d/default.conf

# 将 nginx 持续运行在前台
CMD ["nginx", "-g", "daemon off;"]

# docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f nginx.Dockerfile -t registry-intl.ap-southeast-1.aliyuncs.com/relax_many/web-base-env:v1.0.0 --push .
