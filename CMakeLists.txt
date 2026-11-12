cmake_minimum_required(VERSION 3.20)

project(reactor_user_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_PROTOBUF "Build Protobuf handlers, Linux server, and client" ON)
option(ENABLE_MYSQL "Build MySQL-backed repository when client libraries are available" ON)
option(BUILD_QT_CLIENT "Build the Qt Widgets desktop client" OFF)

find_package(Threads REQUIRED)

if(ENABLE_PROTOBUF)
    find_package(Protobuf REQUIRED)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/user.proto)

    add_library(user_proto ${PROTO_SRCS} ${PROTO_HDRS})
    target_link_libraries(user_proto PUBLIC protobuf::libprotobuf)
    target_include_directories(user_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(ENABLE_MYSQL)
    find_path(MYSQL_INCLUDE_DIR mysql.h
        PATH_SUFFIXES mysql mariadb
    )
    find_library(MYSQL_LIBRARY
        NAMES mysqlclient mariadb libmysql
    )

    if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
        set(HAVE_MYSQL TRUE)
    else()
        set(HAVE_MYSQL FALSE)
        message(WARNING "MySQL client development files were not found. MySQL-backed targets will not be built.")
    endif()
else()
    set(HAVE_MYSQL FALSE)
endif()

add_library(user_service_core
    src/base/Config.cpp
    src/base/Crypto.cpp
    src/base/Logger.cpp
    src/base/ThreadPool.cpp
    src/business/AuditLogRepository.cpp
    src/business/AuditService.cpp
    src/business/FileAuditLogRepository.cpp
    src/business/FileUserRepository.cpp
    src/business/InMemoryAuditLogRepository.cpp
    src/business/InMemoryUserRepository.cpp
    src/business/LoginAttemptTracker.cpp
    src/business/PasswordHasher.cpp
    src/business/AuthSession.cpp
    src/business/SessionRegistry.cpp
    src/business/UserQuery.cpp
    src/business/UserService.cpp
    src/protocol/Codec.cpp
)

target_include_directories(user_service_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(user_service_core
    PUBLIC
        Threads::Threads
)

add_executable(console_demo demo/console_demo.cpp)
target_link_libraries(console_demo PRIVATE user_service_core)

if(ENABLE_PROTOBUF)
    add_library(user_service_handlers
        src/business/UserHandler.cpp
    )
    target_include_directories(user_service_handlers
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(user_service_handlers PUBLIC user_service_core user_proto)

    if(BUILD_TESTS)
        add_executable(test_user_handler tests/test_user_handler.cpp)
        target_link_libraries(test_user_handler PRIVATE user_service_handlers)
        add_test(NAME test_user_handler COMMAND test_user_handler)

        add_executable(test_protocol_operations tests/test_protocol_operations.cpp)
        target_link_libraries(test_protocol_operations PRIVATE user_service_handlers)
        add_test(NAME test_protocol_operations COMMAND test_protocol_operations)

        add_executable(test_handler_extras tests/test_handler_extras.cpp)
        target_link_libraries(test_handler_extras PRIVATE user_service_handlers)
        add_test(NAME test_handler_extras COMMAND test_handler_extras)
    endif()
endif()

if(ENABLE_MYSQL AND HAVE_MYSQL)
    add_library(user_service_mysql
        src/db/MySqlConnection.cpp
        src/db/MySqlConnectionPool.cpp
        src/db/MySqlAuditLogRepository.cpp
        src/db/MySqlUserRepository.cpp
    )
    target_include_directories(user_service_mysql
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${MYSQL_INCLUDE_DIR}
    )
    target_link_libraries(user_service_mysql PUBLIC user_service_core ${MYSQL_LIBRARY})
endif()

if(ENABLE_PROTOBUF AND UNIX AND NOT APPLE)
    add_library(user_service_net
        src/net/Acceptor.cpp
        src/net/Buffer.cpp
        src/net/EventLoop.cpp
        src/net/EventLoopThreadPool.cpp
        src/net/SignalHandler.cpp
        src/net/SocketUtil.cpp
        src/net/TcpConnection.cpp
        src/net/TcpServer.cpp
        src/net/TimerQueue.cpp
    )
    target_include_directories(user_service_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(user_service_net PUBLIC user_service_handlers)

    add_executable(reactor_user_server src/main.cpp)
    target_link_libraries(reactor_user_server PRIVATE user_service_net)
    if(HAVE_MYSQL)
        target_link_libraries(reactor_user_server PRIVATE user_service_mysql)
        target_compile_definitions(reactor_user_server PRIVATE HAVE_MYSQL=1)
    endif()

    add_executable(test_client client/test_client.cpp)
    target_link_libraries(test_client PRIVATE user_service_handlers)

    if(BUILD_TESTS)
        add_executable(test_connection_output tests/test_connection_output.cpp)
        target_link_libraries(test_connection_output PRIVATE user_service_net)
        add_test(NAME test_connection_output COMMAND test_connection_output)
    endif()
endif()

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_codec tests/test_codec.cpp)
    target_link_libraries(test_codec PRIVATE user_service_core)
    add_test(NAME test_codec COMMAND test_codec)

    add_executable(test_threadpool tests/test_threadpool.cpp)
    target_link_libraries(test_threadpool PRIVATE user_service_core)
    add_test(NAME test_threadpool COMMAND test_threadpool)

    add_executable(test_user_service tests/test_user_service.cpp)
    target_link_libraries(test_user_service PRIVATE user_service_core)
    add_test(NAME test_user_service COMMAND test_user_service)

    add_executable(test_file_user_repository tests/test_file_user_repository.cpp)
    target_link_libraries(test_file_user_repository PRIVATE user_service_core)
    add_test(NAME test_file_user_repository COMMAND test_file_user_repository)

    add_executable(test_session_registry tests/test_session_registry.cpp)
    target_link_libraries(test_session_registry PRIVATE user_service_core)
    add_test(NAME test_session_registry COMMAND test_session_registry)

    add_executable(test_crypto tests/test_crypto.cpp)
    target_link_libraries(test_crypto PRIVATE user_service_core)
    add_test(NAME test_crypto COMMAND test_crypto)

    add_executable(test_password_hasher tests/test_password_hasher.cpp)
    target_link_libraries(test_password_hasher PRIVATE user_service_core)
    add_test(NAME test_password_hasher COMMAND test_password_hasher)

    add_executable(test_login_limiter tests/test_login_limiter.cpp)
    target_link_libraries(test_login_limiter PRIVATE user_service_core)
    add_test(NAME test_login_limiter COMMAND test_login_limiter)

    add_executable(test_query_and_audit tests/test_query_and_audit.cpp)
    target_link_libraries(test_query_and_audit PRIVATE user_service_core)
    add_test(NAME test_query_and_audit COMMAND test_query_and_audit)
endif()

if(BUILD_QT_CLIENT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
    add_subdirectory(client/QtUserClient)
endif()
