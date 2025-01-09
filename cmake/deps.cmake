
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ----------------------------------------------------------
# pjproject: Isolate external build, produce INTERFACE target
# ----------------------------------------------------------
set(PJPROJECT_INSTALL_DIR "${CMAKE_BINARY_DIR}/pjproject_install")

include(ExternalProject)
ExternalProject_Add(
        pjproject_ext
        GIT_REPOSITORY https://github.com/pjsip/pjproject.git
        GIT_TAG master
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ./configure --prefix=${PJPROJECT_INSTALL_DIR} --disable-opencore-amr --enable-shared=no --disable-video
        BUILD_COMMAND
        COMMAND make dep
        COMMAND make
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
)

add_library(pjproject INTERFACE)
add_dependencies(pjproject pjproject_ext)
target_include_directories(pjproject INTERFACE ${PJPROJECT_INSTALL_DIR}/include)
target_link_directories(pjproject INTERFACE ${PJPROJECT_INSTALL_DIR}/lib)
target_link_libraries(pjproject INTERFACE
        pjsua2-x86_64-unknown-linux-gnu
        pjsua-x86_64-unknown-linux-gnu
        pjsip-ua-x86_64-unknown-linux-gnu
        pjsip-simple-x86_64-unknown-linux-gnu
        pjsip-x86_64-unknown-linux-gnu
        pjmedia-codec-x86_64-unknown-linux-gnu
        pjmedia-x86_64-unknown-linux-gnu
        pjmedia-audiodev-x86_64-unknown-linux-gnu
        pjmedia-videodev-x86_64-unknown-linux-gnu
        pjnath-x86_64-unknown-linux-gnu
        pjlib-util-x86_64-unknown-linux-gnu
        srtp-x86_64-unknown-linux-gnu
        resample-x86_64-unknown-linux-gnu
        gsmcodec-x86_64-unknown-linux-gnu
        speex-x86_64-unknown-linux-gnu
        ilbccodec-x86_64-unknown-linux-gnu
        g7221codec-x86_64-unknown-linux-gnu
        webrtc-x86_64-unknown-linux-gnu
        pj-x86_64-unknown-linux-gnu
        yuv
        ssl
        crypto
        uuid
        asound
        SDL2
        v4l2
        m
        rt
        pthread
)
# ----------------------------------------------------------
# WebRTC: Local static library build
# ----------------------------------------------------------
set(WEBRTC_DIR "${CMAKE_SOURCE_DIR}/include/deps/webrtc")
file(GLOB_RECURSE WEBRTC_SOURCES "${WEBRTC_DIR}/*.c" "${WEBRTC_DIR}/*.cc")

add_library(my_webrtc STATIC ${WEBRTC_SOURCES})
target_include_directories(my_webrtc PUBLIC
        ${CMAKE_SOURCE_DIR}/include/deps
        ${WEBRTC_DIR}
        ${CMAKE_SOURCE_DIR}/include/deps/webrtc/common_audio/signal_processing/include
)
target_link_libraries(my_webrtc PUBLIC pthread m)


