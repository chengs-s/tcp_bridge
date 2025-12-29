cmake_minimum_required(VERSION 3.15)
project(tcp_bridge_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

add_executable(tcp_bridge_app
    main.cpp
    net_io.cpp
)
target_link_libraries(tcp_bridge_app PRIVATE Threads::Threads)

add_executable(device_server_demo
    demos/device_server/device_server.cpp
)
target_link_libraries(device_server_demo PRIVATE Threads::Threads)

add_executable(upper_client_demo
    demos/upper_client/upper_client.cpp
)
target_link_libraries(upper_client_demo PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(tcp_bridge_app PRIVATE ws2_32)
    target_link_libraries(device_server_demo PRIVATE ws2_32)
    target_link_libraries(upper_client_demo PRIVATE ws2_32)
endif()
