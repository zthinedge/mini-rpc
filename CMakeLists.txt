cmake_minimum_required(VERSION 3.16)

project(mini_rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(minirpc
    src/net/InetAddress.cpp
    src/net/Socket.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/Poller.cpp
    src/net/EventLoop.cpp
    src/net/Acceptor.cpp
    src/net/Connector.cpp
    src/net/TcpConnection.cpp
    src/net/TcpServer.cpp
)

target_include_directories(minirpc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

add_executable(test_socket
    tests/net/testsocket.cpp
)
add_executable(test_buffer
    tests/net/testbuffer.cpp
)
add_executable(test_connector
    tests/net/testconnector.cpp
)

target_link_libraries(test_socket PRIVATE
    minirpc
)
target_link_libraries(test_buffer PRIVATE
    minirpc
)
target_link_libraries(test_connector PRIVATE
    minirpc
)


add_test(NAME test_socket COMMAND test_socket)
add_test(NAME test_buffer COMMAND test_buffer)
add_test(NAME test_connector COMMAND test_connector)
