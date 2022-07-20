project(common)

add_library(common buffer.cpp
                   hash.cpp
                   commitment.cpp
                   hashmap.cpp
                   keys.cpp
                   config.cpp
                   logging.cpp
                   random_source.cpp)
