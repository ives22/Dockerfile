# base image
FROM centos

# MAINTAINER
MAINTAINER 381347268@qq.com

# add epel and 163 yum
RUN yum install wget epel-release -y \
	&& mv /etc/yum.repos.d/CentOS-Base.repo /etc/yum.repos.d/CentOS-Base.repo.back \
	&& wget -P /etc/yum.repos.d/ http://mirrors.aliyun.com/repo/Centos-7.repo \
	&& wget -P /etc/yum.repos.d/ http://mirrors.163.com/.help/CentOS7-Base-163.repo \
	&& yum clean all && yum makecache

# Necessary packages
RUN yum install -y  wget gcc gcc-c++ glibc make autoconf openssl openssl-devel ntpdata crontabs

# change timzone to Asia/Shanghai
RUN cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime

