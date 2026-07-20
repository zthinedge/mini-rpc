cmake_minimum_required(VERSION 3.22)

project(mini_rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Protobuf.cmake)
add_subdirectory(examples/services/calculator)

add_library(minirpc
    src/net/InetAddress.cpp
    src/net/Socket.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/Poller.cpp
    src/net/EventLoop.cpp
    src/net/Acceptor.cpp
    src/net/Connector.cpp
    src/net/TcpClient.cpp
    src/net/TcpConnection.cpp
    src/net/TcpServer.cpp
    src/protocol/RpcHeaderCodec.cpp
    src/protocol/RpcMetaCodec.cpp
    src/protocol/RpcCodec.cpp
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
add_executable(test_tcp
    tests/net/testtcp.cpp
)
add_executable(test_rpc_codec
    tests/protocol/test_rpc_codec.cpp
)
add_executable(test_protobuf
    tests/serialization/test_protobuf.cpp
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
target_link_libraries(test_tcp PRIVATE
    minirpc
)
target_link_libraries(test_rpc_codec PRIVATE
    minirpc
)
target_link_libraries(test_protobuf PRIVATE
    calculator_proto
)


add_test(NAME test_socket COMMAND test_socket)
add_test(NAME test_buffer COMMAND test_buffer)
add_test(NAME test_connector COMMAND test_connector)
add_test(NAME test_tcp COMMAND test_tcp)
add_test(NAME test_rpc_codec COMMAND test_rpc_codec)
add_test(NAME test_protobuf COMMAND test_protobuf)
