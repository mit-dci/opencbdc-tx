project(agent)

add_library(agent impl.cpp
                  interface.cpp
                  server_interface.cpp
                  client.cpp
                  format.cpp)
