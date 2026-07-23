cmake_minimum_required(VERSION 3.22)

project(mini_rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include(cmake/Protobuf.cmake)

add_library(minirpc
    src/cluster/Endpoint.cpp
    src/cluster/RetryPolicy.cpp
    src/cluster/ConnectionPool.cpp
    src/cluster/ChannelManager.cpp
    src/log/AsyncLogger.cpp
    src/log/RollingFileSink.cpp
    src/metrics/RpcMetrics.cpp
    src/net/InetAddress.cpp
    src/net/Socket.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/Poller.cpp
    src/net/EventLoop.cpp
    src/net/TimerQueue.cpp
    src/net/Acceptor.cpp
    src/net/Connector.cpp
    src/net/TcpClient.cpp
    src/net/TcpConnection.cpp
    src/net/TcpServer.cpp
    src/protocol/RpcHeaderCodec.cpp
    src/protocol/RpcMetaCodec.cpp
    src/protocol/RpcCodec.cpp
    src/rpc/ServiceDispatcher.cpp
    src/rpc/RpcServer.cpp
    src/rpc/PendingCalls.cpp
    src/rpc/RpcClient.cpp
)

target_include_directories(minirpc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(minirpc PUBLIC
    Threads::Threads
)

add_subdirectory(examples/services/calculator)

add_executable(async_logger_demo
    examples/logging/async_logger_demo.cpp
)
target_link_libraries(async_logger_demo PRIVATE
    minirpc
)

add_executable(calculator_server
    examples/server/calculator_server.cpp
)
target_link_libraries(calculator_server PRIVATE
    calculator_rpc
)

add_executable(calculator_client
    examples/client/calculator_client.cpp
)
target_link_libraries(calculator_client PRIVATE
    calculator_rpc
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
add_executable(test_event_loop
    tests/net/testeventloop.cpp
)
add_executable(test_rpc_codec
    tests/protocol/test_rpc_codec.cpp
)
add_executable(test_protobuf
    tests/serialization/test_protobuf.cpp
)
add_executable(test_rpc_server
    tests/rpc/test_rpc_server.cpp
)
add_executable(test_rpc_client
    tests/rpc/test_rpc_client.cpp
)
add_executable(test_async_logger
    tests/log/test_async_logger.cpp
)
add_executable(test_connection_pool
    tests/cluster/test_connection_pool.cpp
)
add_executable(test_rpc_metrics
    tests/metrics/test_rpc_metrics.cpp
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
target_link_libraries(test_event_loop PRIVATE
    minirpc
)
target_link_libraries(test_rpc_codec PRIVATE
    minirpc
)
target_link_libraries(test_protobuf PRIVATE
    calculator_proto
)
target_link_libraries(test_rpc_server PRIVATE
    minirpc
)
target_link_libraries(test_rpc_client PRIVATE
    calculator_rpc
)
target_link_libraries(test_async_logger PRIVATE
    minirpc
)
target_link_libraries(test_connection_pool PRIVATE
    minirpc
)
target_link_libraries(test_rpc_metrics PRIVATE
    minirpc
)


add_test(NAME test_socket COMMAND test_socket)
add_test(NAME test_buffer COMMAND test_buffer)
add_test(NAME test_connector COMMAND test_connector)
add_test(NAME test_tcp COMMAND test_tcp)
add_test(NAME test_event_loop COMMAND test_event_loop)
add_test(NAME test_rpc_codec COMMAND test_rpc_codec)
add_test(NAME test_protobuf COMMAND test_protobuf)
add_test(NAME test_rpc_server COMMAND test_rpc_server)
add_test(NAME test_rpc_client COMMAND test_rpc_client)
add_test(NAME test_async_logger COMMAND test_async_logger)
add_test(NAME test_connection_pool COMMAND test_connection_pool)
add_test(NAME test_rpc_metrics COMMAND test_rpc_metrics)
