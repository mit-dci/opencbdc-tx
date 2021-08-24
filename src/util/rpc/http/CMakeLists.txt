add_library(json_rpc_http json_rpc_http_client.cpp
                          json_rpc_http_server.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(json_rpc_http PRIVATE kqueue_event_handler.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(json_rpc_http PRIVATE epoll_event_handler.cpp)
endif()
