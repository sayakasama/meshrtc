cmake_minimum_required(VERSION 3.10)
project(rtc_client C CXX)

# =============================================================================
# 1. 变量定义 (对应 Makefile 的路径)
# =============================================================================
# 获取当前 CMakeLists.txt 所在目录作为 root_path
set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_PATH "${ROOT_PATH}/gbcom_common")
set(SOAP_PATH   "${ROOT_PATH}/gbcom_common/comm/rpc")
set(BIN_PATH    "${ROOT_PATH}/bin")

# WebRTC 和第三方库路径 (注意：这里用了你原本 Makefile 里的路径变量)
set(THIRD_PARTY_PATH "/home/meshrtc_new/other_release")
set(WEB_INC_PATH     "/home/meshrtc_new/includes/webrtc_headers")
# 注意：这里改用我们新编译出来的 libwebrtc_full.a 的路径
# 假设你把新库放在了这里，如果不是请修改！
set(WEBRTC_LIB         "/home/meshrtc_new/libs/libwebrtc.a")
set(WEBRTC_EXTRA_LIB   "/home/meshrtc_new/libs/libwebrtc_extra.a")
set(CURL_LIB           "/home/meshrtc_new/libs/libcurl.a")
# =============================================================================
# 2. 头文件包含 (对应 Makefile 的 include_path + CXXFLAGS -I)
# =============================================================================
include_directories(
    # 本地目录
    ${ROOT_PATH}/audio
    ${ROOT_PATH}/video
    ${COMMON_PATH}
    ${COMMON_PATH}/h264
    ${COMMON_PATH}/pcm
    ${COMMON_PATH}/queue
    ${COMMON_PATH}/json
    ${COMMON_PATH}/comm
    ${COMMON_PATH}/comm/ipc
    ${COMMON_PATH}/mesh_call
    ${SOAP_PATH}
    
    # 第三方库头文件
    ${THIRD_PARTY_PATH}/include
    
    # WebRTC 头文件
    ${WEB_INC_PATH}
    ${WEB_INC_PATH}/rtc_base
    ${WEB_INC_PATH}/third_party/abseil-cpp
    ${WEB_INC_PATH}/third_party/libyuv/include
    ${WEB_INC_PATH}/third_party/jsoncpp/source/include
)

# =============================================================================
# 3. 源文件集合 (对应 Makefile 的 OBJS_... 和 wildcard)
# =============================================================================

# 3.1 自动扫描的目录 (*.cc)
file(GLOB ROOT_SRCS    "${ROOT_PATH}/*.cc")
file(GLOB AUDIO_SRCS   "${ROOT_PATH}/audio/*.cc")
file(GLOB VIDEO_SRCS   "${ROOT_PATH}/video/*.cc")
file(GLOB ENCODER_SRCS "${ROOT_PATH}/encoder/*.cc")

# 3.2 手动指定的源文件 (Common / Utils)
# 注意：CMake 会自动根据扩展名 (.c/.cc/.cpp) 选择编译器
set(UTIL_SRCS
    ${COMMON_PATH}/utils.c      # 假设是 .c，如果是 .cc 请修改后缀
    ${COMMON_PATH}/cfg.c
    ${COMMON_PATH}/base64.c
    ${COMMON_PATH}/mesh_call/mesh_rtc_call.c
)

set(PCM_SRCS
    ${COMMON_PATH}/pcm/pcm.c
    ${COMMON_PATH}/pcm/pcm_audio_dev.c
)

set(JSON_COMMON_SRCS
    ${COMMON_PATH}/json/cjson.c
    ${COMMON_PATH}/json/json_codec.c
)

set(COMM_SRCS
    ${COMMON_PATH}/comm/udp_comm.c
    ${COMMON_PATH}/comm/rtp_session.c
    ${COMMON_PATH}/comm/rtp_session_thread.c
    ${COMMON_PATH}/comm/multicast.c
    ${COMMON_PATH}/comm/rtp_multicast.c
    ${COMMON_PATH}/comm/http/http_client.c
    ${COMMON_PATH}/comm/http/http_thread.c
    ${COMMON_PATH}/comm/websocket/websocket_client.c
    ${COMMON_PATH}/comm/websocket/websocket_thread.c
)

set(IPC_SRCS
    ${COMMON_PATH}/comm/ipc/ipc_msg.c
    ${COMMON_PATH}/comm/ipc/ipc_thread.c
)

set(QUEUE_SRCS
    ${COMMON_PATH}/queue/circular_queue.c
)

# SOAP 生成代码
set(RPC_CLIENT_SRCS
    ${SOAP_PATH}/soapClient.c # 或 .cpp
    ${SOAP_PATH}/soapC.c
    ${SOAP_PATH}/stdsoap2.c
    ${SOAP_PATH}/client.c
)

# 合并所有源文件
set(ALL_SRCS
    ${ROOT_SRCS} ${JSON_SRCS} ${AUDIO_SRCS} ${VIDEO_SRCS} ${ENCODER_SRCS}
    ${UTIL_SRCS} ${PCM_SRCS} ${JSON_COMMON_SRCS} ${COMM_SRCS}
    ${IPC_SRCS} ${QUEUE_SRCS} ${RPC_CLIENT_SRCS}
)

# =============================================================================
# 4. 生成可执行文件
# =============================================================================
add_executable(rtc_client ${ALL_SRCS})

# =============================================================================
# 5. 链接库配置
# =============================================================================
# 添加库搜索路径 (-L)
link_directories(
    ${THIRD_PARTY_PATH}/lib
    # WebRTC 库路径如果不只有 full.a 还有别的，可以在这里加
)

# 链接具体的库
target_link_libraries(rtc_client PRIVATE
    # 1. 你的新 WebRTC 库 (直接指定全路径)
    ${WEBRTC_LIB}
    ${WEBRTC_EXTRA_LIB}
    # 2. 其他依赖库
    ${CURL_LIB}
    # 3. 系统库 (对应 -lpthread -ldl -lm -latomic)
    # toolchain.cmake 里已经处理了 libc/libstdc++，这里只需要列出名字
    pthread
    m
    atomic
    rt
    # dl 库在静态链接时通常不需要，如果代码里有dlopen桩，这里可以去掉
)

# =============================================================================
# 6. 安装/拷贝 (对应 Makefile 的 cp 命令)
# =============================================================================
# 编译完成后自动将 rtc_client 拷贝到 ../../bin 目录
add_custom_command(TARGET rtc_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rtc_client> ${BIN_PATH}/rtc_client
    COMMENT "Copying binary to ${BIN_PATH}"
)