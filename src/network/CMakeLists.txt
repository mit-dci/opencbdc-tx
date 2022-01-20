project(network)

add_library(network connection_manager.cpp
                    peer.cpp
                    socket.cpp
                    socket_selector.cpp
                    tcp_listener.cpp
                    tcp_socket.cpp)
