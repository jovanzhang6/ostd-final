# oscd/Dockerfile

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV USER=root

# 更换阿里云镜像源加速下载
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装编译工具、内核头文件、bear 及依赖库
RUN apt-get update && apt-get install -y \
    build-essential \
    libreadline-dev \
    libfuse-dev \
    linux-headers-generic \
    bear \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /oscd

# 复制项目全部源码（使用 .dockerignore 排除不需要的文件）
COPY . .

# 编译用户态程序
RUN cd oscdsh && make
RUN cd oscdfs && make

# 尝试编译内核驱动（若内核头文件版本不匹配则跳过，不会中断构建）
RUN cd oscddrv && make || echo "oscddrv build skipped (kernel headers mismatch)"

# 统一收集可执行文件与内核模块
RUN mkdir -p /oscd/bin && \
    cp oscdsh/oscdsh /oscd/bin/ && \
    cp oscdfs/oscdfs /oscd/bin/ && \
    if [ -f oscddrv/oscddrv.ko ]; then cp oscddrv/oscddrv.ko /oscd/bin/; fi

ENV PATH="/oscd/bin:${PATH}"

CMD ["oscdsh"]