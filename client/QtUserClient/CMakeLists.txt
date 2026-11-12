set(QT_CLIENT_SOURCES
    main.cpp
    QtTcpClient.cpp
    QtTcpClient.h
    ProtoMessages.cpp
    ProtoMessages.h
    ClientSession.cpp
    ClientSession.h
    LoginWidget.cpp
    LoginWidget.h
    RegisterWidget.cpp
    RegisterWidget.h
    DashboardWidget.cpp
    DashboardWidget.h
    MainWindow.cpp
    MainWindow.h
)

add_executable(QtUserClient ${QT_CLIENT_SOURCES})
set_target_properties(QtUserClient PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE ON
    qt_no_entrypoint TRUE
)
target_include_directories(QtUserClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(QtUserClient PRIVATE
    Qt6::Widgets
    Qt6::Network
    user_service_core
)

if(BUILD_TESTS)
    add_executable(test_qt_proto_messages ProtoMessages.test.cpp ProtoMessages.cpp ProtoMessages.h)
    target_link_libraries(test_qt_proto_messages PRIVATE Qt6::Core)
    add_test(NAME test_qt_proto_messages COMMAND test_qt_proto_messages)

    add_executable(test_client_session ClientSession.test.cpp ClientSession.cpp ClientSession.h)
    target_link_libraries(test_client_session PRIVATE Qt6::Core)
    add_test(NAME test_client_session COMMAND test_client_session)

    add_executable(test_dashboard_widget DashboardWidget.test.cpp DashboardWidget.cpp DashboardWidget.h)
    set_target_properties(test_dashboard_widget PROPERTIES AUTOMOC ON)
    target_link_libraries(test_dashboard_widget PRIVATE Qt6::Widgets)
    add_test(NAME test_dashboard_widget COMMAND test_dashboard_widget)

    add_executable(test_login_widget LoginWidget.test.cpp LoginWidget.cpp LoginWidget.h)
    set_target_properties(test_login_widget PROPERTIES AUTOMOC ON)
    target_link_libraries(test_login_widget PRIVATE Qt6::Widgets)
    add_test(NAME test_login_widget COMMAND test_login_widget)

    add_executable(test_register_widget RegisterWidget.test.cpp RegisterWidget.cpp RegisterWidget.h)
    set_target_properties(test_register_widget PROPERTIES AUTOMOC ON)
    target_link_libraries(test_register_widget PRIVATE Qt6::Widgets)
    add_test(NAME test_register_widget COMMAND test_register_widget)
endif()
