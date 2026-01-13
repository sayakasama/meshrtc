# toolchain.cmake - WebRTC Clang Cross Compilation Setup
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# =============================================================================
# 1. 配置路径 (请根据你的实际情况修改 WEBRTC_ROOT)
# =============================================================================
set(WEBRTC_ROOT "/home/meshrtc_new")  # 修改为你的 WebRTC 根目录路径
set(LLVM_BIN "${WEBRTC_ROOT}/llvm-build/Release+Asserts/bin")

# 指定编译器为 WebRTC 自带的 Clang
set(CMAKE_C_COMPILER "${LLVM_BIN}/clang")
set(CMAKE_CXX_COMPILER "${LLVM_BIN}/clang++")

# 指定链接器为 LLD (解决 CREL 格式兼容性问题)
set(CMAKE_LINKER "${LLVM_BIN}/ld.lld")

# =============================================================================
# 2. 自动查找本机的系统静态库 (libstdc++.a 和 libc.a)
#    这是为了解决 Clang 找不到 GCC 交叉编译环境库的问题
# =============================================================================
#这里不找了，直接设置好
set(SYS_CPP_LIB_PATH "/usr/aarch64-linux-gnu/lib/")
# 查找 libstdc++.a
# execute_process(
#     COMMAND sh -c "find /usr -name 'libstdc++.a' 2>/dev/null | grep 'aarch64' | head -n 1"
#     OUTPUT_VARIABLE SYS_CPP_LIB_FILE
#     OUTPUT_STRIP_TRAILING_WHITESPACE
# )
# get_filename_component(SYS_CPP_LIB_PATH "${SYS_CPP_LIB_FILE}" DIRECTORY)

# # 查找 libc.a
# execute_process(
#     COMMAND sh -c "find /usr -name 'libc.a' 2>/dev/null | grep 'aarch64' | head -n 1"
#     OUTPUT_VARIABLE SYS_C_LIB_FILE
#     OUTPUT_STRIP_TRAILING_WHITESPACE
# )
# get_filename_component(SYS_C_LIB_PATH "${SYS_C_LIB_FILE}" DIRECTORY)

# message(STATUS "Found System C++ Lib Path: ${SYS_CPP_LIB_PATH}")
# message(STATUS "Found System C Lib Path:   ${SYS_C_LIB_PATH}")

# =============================================================================
# 3. 注入编译和链接参数
# =============================================================================
# 基础架构参数
set(ARCH_FLAGS "--target=aarch64-linux-gnu --sysroot=/")

# C/C++ 通用 Flags
set(CMAKE_C_FLAGS   "${ARCH_FLAGS} -Wall -O2 -DNO_ANDROID" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${ARCH_FLAGS} -std=c++17 -fno-rtti -Wall -O2 -DNO_ANDROID -DWEBRTC_POSIX" CACHE STRING "" FORCE)

# 链接 Flags (核心：静态链接 + 显式指定库路径)
# 注意：这里我们把系统库路径直接加到了 LINKER_FLAGS 里
set(CMAKE_EXE_LINKER_FLAGS "${ARCH_FLAGS} -static -fuse-ld=lld -L${SYS_CPP_LIB_PATH}" CACHE STRING "" FORCE)