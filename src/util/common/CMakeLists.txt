project(common)

add_library(common buffer.cpp
                   hash.cpp
                   hashmap.cpp
                   keys.cpp
                   config.cpp
                   logging.cpp
                   random_source.cpp
                   thread_pool.cpp)
