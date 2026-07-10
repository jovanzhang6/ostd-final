# 使用 Ubuntu 22.04 作为基础镜像
FROM ubuntu:22.04

# 避免交互式安装时的提示
ENV DEBIAN_FRONTEND=noninteractive
# 默认是root
ENV USER=root

# 更换为阿里云镜像源（加速 apt 更新和下载）
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装编译工具和 readline 库（oscdsh 依赖）
RUN apt-get update && apt-get install -y \
    build-essential \
    libreadline-dev \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /oscd

# 将整个项目复制到容器中（实际只会复制未被 .dockerignore 排除的文件）
COPY . .

# 进入实验一目录并编译
RUN cd oscdsh && make

# 将编译好的 oscdsh 所在目录加入 PATH，方便直接运行
ENV PATH="/oscd/oscdsh:${PATH}"

# 容器启动时使用完整路径运行 oscdsh（确保 PATH 生效前也能启动）
CMD ["/oscd/oscdsh/oscdsh"]