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
        git \
        curl \
        zip \
        unzip \
        tar \
        pkg-config

RUN git config --global http.version HTTP/1.1

RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh && \
    ln -s /opt/vcpkg/vcpkg /usr/local/bin/vcpkg

RUN vcpkg install websocketpp

ENV VCPKG_ROOT=/opt/vcpkg
ENV CMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake


WORKDIR /app
COPY . /app

RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} .. && \
    make

# Default command
CMD ["./build/server"]

