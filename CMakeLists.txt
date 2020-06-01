cmake_minimum_required(VERSION 3.10)
project(system_calls C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(system_calls
        device.h
        device.c
        client.c
        defines.c
        defines.h
        err_exit.c
        err_exit.h
        fifo.c
        fifo.h
        semaphore.c
        semaphore.h
        server.c
        shared_memory.c
        shared_memory.h
        ackmanager.h
        ackmanager.c
        device.c
        device.h)
