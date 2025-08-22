FROM ubuntu:22.04 AS builder

RUN apt update && \
    apt install -y build-essential cmake git libssl-dev && \
    apt clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

#RUN rm -rf /app/build

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCPACK_INCLUDE_LIBS=OFF .. && \
    make -j$(nproc) && \
    strip bin/mcp-server++

# -----------------------------------------------------------------------------------#
FROM debian:12-slim

RUN echo 'deb https://mirrors.ustc.edu.cn/debian bookworm main' > /etc/apt/sources.list && \
    echo 'deb https://mirrors.ustc.edu.cn/debian bookworm-updates main' >> /etc/apt/sources.list && \
    echo 'deb https://mirrors.ustc.edu.cn/debian-security bookworm-security main' >> /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libssl3 \
        ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/bin/mcp-server++ /mcp-server++
COPY config.ini.example /config.ini

RUN mkdir -p /logs /plugins

EXPOSE 6666 6667

CMD ["/mcp-server++"]