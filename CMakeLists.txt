cmake_minimum_required(VERSION 3.28)
project(LagDeniedServer C)

set(CMAKE_C_STANDARD 17)

add_executable(LagDeniedServer main.c utils.c
        protocol.h
        protocol.c
        bufpool.h
        bufpool.c
        banner.h
        version.h)
target_link_libraries(LagDeniedServer PRIVATE uv)