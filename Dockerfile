FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
# Install dependencies
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        libgtest-dev \
        libopencore-amrnb-dev \
        libopencore-amrwb-dev \
        libboost-all-dev \
        libasio-dev \
        libyuv-dev \
        libssl-dev \
        libasound2-dev \
        libsdl2-dev \
        libv4l-dev \
        libpulse-dev \
        libsrtp2-dev \
        libcurl4-openssl-dev \
        libunbound-dev \
        uuid-dev \
        git

WORKDIR /app
COPY . /app

RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

# Default command
CMD ["./build/server"]

