# Build and test environment for the Linux emulator (Ubuntu 24.04, runs natively on arm64 or x86_64).
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates python3 python3-numpy pkg-config \
        libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxext-dev libxrandr-dev \
        libxinerama-dev libxcursor-dev libxcomposite-dev libxi-dev libxfixes-dev libxrender-dev libgl1-mesa-dev libcurl4-openssl-dev libssl-dev \
        libjack-jackd2-dev libpng-dev \
        xvfb x11-utils imagemagick xauth \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
