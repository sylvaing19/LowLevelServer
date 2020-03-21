cmake_minimum_required(VERSION 3.10)
project(LowLevelServer)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(LowLevelServer
        main.cpp LowLevelMessage.cpp LowLevelMessage.h SocketInterface.cpp SocketInterface.h SerialInterface.cpp SerialInterface.h MessageRouter.cpp MessageRouter.h Pause.cpp Pause.h)
