# 使用 Ubuntu 22.04 作为基础镜像
FROM ubuntu:22.04

# 避免交互式安装时的提示
ENV DEBIAN_FRONTEND=noninteractive
# 默认是root
ENV USER=root

# 更换为阿里云镜像源（加速 apt 更新和下载）
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装编译工具、readline（oscdsh 依赖）、fuse（oscdfs 依赖）
RUN apt-get update && apt-get install -y \
    build-essential \
    libreadline-dev \
    libfuse-dev \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /oscd

# 将整个项目复制到容器中（实际只会复制未被 .dockerignore 排除的文件）
COPY . .

# 编译实验一 Shell
RUN cd oscdsh && make

# 编译实验二 文件系统
RUN cd oscdfs && make

# 将两个可执行文件复制到统一的 bin 目录，方便直接运行
RUN mkdir -p /oscd/bin && \
    cp oscdsh/oscdsh /oscd/bin/ && \
    cp oscdfs/oscdfs /oscd/bin/

# 将 bin 目录加入 PATH
ENV PATH="/oscd/bin:${PATH}"

# 容器启动时默认运行 oscdsh，也可通过 docker run ... oscdfs 来运行文件系统
CMD ["oscdsh"]