FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    meson ninja-build gcc pkg-config \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libzstd-dev \
    gstreamer1.0-tools zstd

WORKDIR /app
COPY . .

RUN meson setup build && meson compile -C build