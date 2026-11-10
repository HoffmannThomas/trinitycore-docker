# syntax=docker/dockerfile:1.7
# Dockerfile - builds TrinityCore (master) from source
# Multi-stage: builder (build tools layer) + runtime (minimal)
# Build-cache persistence: use `docker compose build --build-arg` layers + Docker layer cache.
# For incremental rebuilds across `docker compose build` runs, add named volumes for /ccache and /src/build.

# ---------------- Builder stage ----------------
FROM debian:trixie AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Layer 1: OS base + MySQL apt repository
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget gnupg lsb-release curl \
    && wget -q https://dev.mysql.com/get/mysql-apt-config_0.8.36-1_all.deb -O /tmp/mysql-apt-config_all.deb \
    && DEBIAN_FRONTEND=noninteractive dpkg -i /tmp/mysql-apt-config_all.deb \
    && apt-get update

# Layer 2: all build tools (separate layer so source changes don't invalidate it)
RUN apt-get install -y --no-install-recommends \
        git clang cmake make gcc g++ ccache \
        libmysqlclient-dev libssl-dev libbz2-dev libreadline-dev libncurses-dev \
        libboost-all-dev p7zip \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang 100 \
    && rm -rf /var/lib/apt/lists/*

ENV CCACHE_DIR=/ccache
ENV CCACHE_MAXSIZE=10G
ENV CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime

WORKDIR /src

# Source (invalidates on change)
COPY . /src

# Apply custom patches from docker/patches/
COPY docker/patches /patches
RUN cd /src && for p in /patches/*.patch; do \
        if [ -f "$p" ]; then \
            echo "Applying $p..."; \
            git apply "$p" 2>&1 || echo "Patch $p failed (may already be applied)"; \
        fi; \
    done

# Configure + build + install
RUN --mount=type=cache,target=/ccache \
    --mount=type=cache,target=/src/build \
    mkdir -p /src/build /ccache && cd /src/build && \
    cmake .. \
        -DCMAKE_INSTALL_PREFIX=/opt/trinitycore \
        -DTOOLS=1 \
        -DWITH_WARNINGS=0 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache && \
    make -j$(nproc) && \
    make install

# ---------------- Runtime stage ----------------
FROM debian:trixie AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Runtime dependencies only (build tools not included)
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget gnupg lsb-release curl \
    && wget -q https://dev.mysql.com/get/mysql-apt-config_0.8.36-1_all.deb -O /tmp/mysql-apt-config_all.deb \
    && DEBIAN_FRONTEND=noninteractive dpkg -i /tmp/mysql-apt-config_all.deb \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        mysql-client \
        libssl3 libbz2-1.0 libreadline8 libncurses6 zlib1g \
        libboost-all-dev \
        p7zip \
    && rm -rf /var/lib/apt/lists/*

# Built artifacts
COPY --from=builder /opt/trinitycore /opt/trinitycore

# Runtime MySQL client shared library (linked in builder stage)
COPY --from=builder /usr/lib/x86_64-linux-gnu/libmysqlclient.so* /usr/lib/x86_64-linux-gnu/

# Source SQL (needed by AutoSetup/updates)
COPY --from=builder /src/sql /opt/trinitycore/sql

# Docker helpers (entrypoint)
COPY docker /opt/trinitycore/docker

VOLUME ["/opt/trinitycore/data", "/opt/trinitycore/etc", "/data", "/ccache", "/src/build"]

CMD ["/bin/bash"]
