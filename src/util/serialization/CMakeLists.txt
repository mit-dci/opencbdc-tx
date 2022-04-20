project(serialization)

add_library(serialization format.cpp
                          buffer_serializer.cpp
                          size_serializer.cpp
                          stream_serializer.cpp
                          istream_serializer.cpp
                          ostream_serializer.cpp
                          hamilton.pb.cpp)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds -Wno-class-memaccess")
